/* ctron_net.c - Ctron std/net shim (posix + winsock, same facade)
 * §11.3 defaults: SO_REUSEADDR at listen; TCP_NODELAY + SO_KEEPALIVE applied at
 * accept/connect (create sites). fd travels as int64_t everywhere.
 *
 * 边界口径(Task 4/6 发射面登记):
 *   - 出参唯一通道 = Box64* 镜像(struct Box64 { var v: I64 },声明序布局);
 *   - 缓冲 = &I64[] 视图 lane(每字节占一条 int64 lane;发射器仅预发
 *     i/6/b/s/f 五种视图 typedef,I8/U8 视图是缺口,见 Task 4 登记);
 *   - C-owned 串返回经 thread-local 静态缓冲,Ctron 侧 str_from_c 深拷;
 *   - 错误统一置 thread-local errno 槽(ct_err/ct_werr)后返回 -1,
 *     Ctron 侧经 ctron_net_last_errno / ctron_net_strerror 取详情。
 * P2-C 混合化:五停车点(read_t / write·write_str 发送核 / sleep_ms /
 * udp_recvfrom / shutdown 写等待 —— 最后一处 P1 无 EAGAIN 面,无点可改)
 * 见下方"协程停车面"注;裸线程(未链 rt)P1 原路径逐字节不变。
 * P3-E:AF_UNIX listen/accept/connect/unlink 四件(POSIX 专面)与 DNS
 * 异步化(resolve 协程面入 2 线程 helper 池 + done 槽,不再滞留 worker)
 * 见各段头注。
 */
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* P2-C 收账:rt 垫底三符号取冻结声明自 ctron_rt.h(强定义同头),防 ABI 漂移
 * (签名失配时编译期即报,不再静默弱顶弱) */
#include "ctron_rt.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET ct_sock;
typedef int ct_socklen;
typedef int ct_ssize_t;
#define CT_TLS __declspec(thread)
#define ct_close(fd) closesocket(fd)
#define ct_poll WSAPoll
#define CT_SHUT_WR SD_SEND
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#else
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
typedef int ct_sock;
typedef socklen_t ct_socklen;
typedef ssize_t ct_ssize_t;
#define CT_TLS _Thread_local
#define ct_close(fd) close(fd)
#define ct_poll poll
#define CT_SHUT_WR SHUT_WR
#endif

static CT_TLS int64_t ct_net_errno_v = 0;

/* P3-A 探针消除:协程停车面以 MSG_DONTWAIT 直试 recv/send/recvfrom 替代
 * poll(0) 探针(per-call 非阻塞 —— fd 本体阻塞属性不动,P1 裸面逐字节不变;
 * EAGAIN/EWOULDBLOCK 即无数据信号,停车等就绪后重试)。主目标平台
 * darwin/linux/BSD 均有此旗标;缺面平台退化为 0(登记:该形态下协程路径
 * 首试可能阻塞 worker,移植时须以 O_NONBLOCK 面补齐)。 */
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

/* errno 槽一律经 noinline 访问器读写:darwin/arm64 clang 会把 _Thread_local
 * 的 TLV 槽位解析结果缓存在 callee-saved 寄存器里,而协程跨 worker 迁移后
 * rt_swap 恢复的是旧线程的寄存器镜像 ⇒ 直读/直写槽位可能命中别的线程的块
 * (P2-C 实证:ETIMEDOUT 写入旧块,last_errno 读到 0)。noinline 强制每次
 * 调用在当前线程重新解析。ctron_rt.c 同款约束与对策,已登记。 */
__attribute__((noinline)) static int64_t* ct_err_slot(void) { return &ct_net_errno_v; }

int64_t ctron_net_last_errno(void) { return *ct_err_slot(); }

#ifdef _WIN32
#define ct_err() (*ct_err_slot() = (int64_t)WSAGetLastError(), -1)
#else
static int64_t ct_err(void) { *ct_err_slot() = (int64_t)errno; return -1; }
#endif

/* Box64 镜像 Ctron struct Box64 { var v: I64 }(声明序 = C 声明序,单 I64 字段;
 * 独立 TU 布局同型即 ABI 兼容 —— tests/ffi/repr_c 先例) */
typedef struct { int64_t v; } Box64;

/* I64 视图 lane 镜像:&I64[] 发射 ctron_view_6 { d, n } 按值(§9.6) */
typedef struct { int64_t* d; int64_t n; } ct_view6;

/* 视图 lane 缓冲一次搬运上限 4096(栈上暂存):TCP 写路径经分块循环,超限
 * 分次 send 不丢;UDP sendto 超 4096 即按 4096 截断为单报文(EMSGSIZE 守卫
 * 列 P2),recvfrom/read 同限单调用;当前所有 P1 消费面消息均 ≤2 字节,未触界。 */
#define CT_CHUNK 4096

#ifdef _WIN32
static int ct_wsa_once(void) {
    static WSADATA wsa;
    static int wsa_ok = 0;
    if (!wsa_ok) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            *ct_err_slot() = (int64_t)WSAGetLastError();
            return -1;
        }
        wsa_ok = 1;
    }
    return 0;
}
#define CT_WSA() ct_wsa_once()
#else
#define CT_WSA() 0
#endif

/* ---- P2-C 协程停车面(net 垫片混合化) ----
 * ctron_rt 三符号以"弱定义哑元"垫底:未链 rt 时即此哑元(6 夹具只链本文件);
 * 链入 rt 时 ctron_rt.c 的强定义在链接期整体顶替弱定义(ELF/Mach-O 标准语义,
 * darwin arm64 -O1/-O2 已实证:哑元态判据恒 0、强链态判据在协程内为 1 且无
 * 同 TU 常量折叠)。为何不用 weak extern 声明:Mach-O 无 undefined-weak→NULL
 * 链接语义(weak/weak_import 纯 extern 声明均链接期报 undefined,已实证),
 * 弱定义强顶弱是双侧唯一免链接旗标的机制。
 * 停车判据(冻结口径)= wait_fd 已链 && current() 非空:哑元 current 恒 NULL
 * ⇒ 判据恒假 ⇒ P1 原路径逐字节不变(6 夹具回归门);判据为真仅当真 rt 已链
 * 且当前处于协程上下文。
 * 限制登记:rt 若经静态库归档链接且无其他拉入引用,弱垫底不被顶替(静默回退
 * P1,不致错);本仓 rt 一律以源/.o 直链(c_src/*.c glob),不受影响。
 * 哑元无条件发射(P2 终审收账):_WIN32 无 rt(POSIX-only),但 sleep_ms/
 * wait_fd 的调用点只有运行期守卫(ct_rt_*_parkable 于 _WIN32 恒 0,仅 -O1
 * 常量折叠消亡),-O0 下符号引用存活 → mingw 链接断;rt 永不在 _WIN32 链入,
 * 哑元即终解,与发射模板侧九哑元无条件发射(e93bde9)同构。 */
__attribute__((weak)) void ctron_rt_wait_fd(int fd, int write_side, int64_t timeout_ms) {
    (void)fd; (void)write_side; (void)timeout_ms;
}
__attribute__((weak)) void* ctron_rt_current(void) { return 0; }
__attribute__((weak)) void ctron_rt_sleep_ms(int64_t ms) { (void)ms; }
/* P3-A 兴趣驻留配套:fd 关闭钩子(摘 rt 驻留登记)。哑元无条件发射,同上。 */
__attribute__((weak)) void ctron_rt_forget_fd(int64_t fd) { (void)fd; }
/* P3-E DNS 异步化配套:park/wake 两符号同款弱垫底(resolve 协程停车等
 * helper 池完成用)。哑元无条件发射(同上收账口径:_WIN32 恒不链 rt 且
 * 调用点在 ct_rt_parkable 运行期守卫内,-O0 下符号引用存活)。 */
__attribute__((weak)) void ctron_rt_park(void) { }
__attribute__((weak)) void ctron_rt_wake(void* key) { (void)key; }

/* 停车判据:垫片符号已链(非哑元态不可能是真,哑元 current 恒 NULL)+ 协程上下文 */
static int ct_rt_parkable(void) {
#if !defined(_WIN32)
    return (ctron_rt_wait_fd != 0) && (ctron_rt_current() != 0);
#else
    return 0;
#endif
}

static int ct_rt_sleep_parkable(void) {
#if !defined(_WIN32)
    return (ctron_rt_sleep_ms != 0) && (ctron_rt_current() != 0);
#else
    return 0;
#endif
}

/* §11.3 TCP 默认面:accept/connect 出口统一开 NODELAY + KEEPALIVE */
static void ct_tcp_defaults(ct_sock fd) {
#ifdef _WIN32
    BOOL one = 1;
#else
    int one = 1;
#endif
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&one, sizeof(one));
}

/* 主机名 → IPv4:数值点分快路径(inet_pton),否则 getaddrinfo(AF_INET)
 * 首个结果(P1 只做 v4;v6 待 P2 扩面) */
static int ct_host4(const char* host, struct in_addr* out) {
    memset(out, 0, sizeof(*out));
    if (inet_pton(AF_INET, host, out) == 1) return 1;
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL) return 0;
    *out = ((struct sockaddr_in*)(void*)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return 1;
}

int64_t ctron_net_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    /* M-T5-1 重排:先除后余,避免 t*1e9 溢出(t/f 为整除,t%f 为余数) */
    return (int64_t)(t.QuadPart / f.QuadPart) * 1000000000LL
         + (int64_t)((t.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { *ct_err_slot() = errno; return -1; }
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

void ctron_net_sleep_ms(int64_t ms) {
    /* P2-C 停车点④:协程上下文(且 rt 已链)→ 定时器堆停车(worker 不滞留);
     * 裸线程 → 原 nanosleep 回退,逐字节不变。 */
    if (ct_rt_sleep_parkable()) {
        ctron_rt_sleep_ms(ms);
        return;
    }
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) { }
#endif
}

/* 协程停车等待 fd 就绪/超时:deadline=0 表永久(rt 口径 timeout<0)。
 * 返回 1 = 已停车并醒(调用方重试 syscall —— 就绪是提示非保证,冻结契约);
 * 返回 0 = 已到 deadline(调用方置 ETIMEDOUT 返回)。 */
static int ct_rt_park_until(int64_t fd, int write_side, uint64_t deadline) {
    int64_t rem = -1;
    if (deadline != 0) {
        uint64_t now = (uint64_t)ctron_net_now_ns();
        if (now >= deadline) return 0;
        rem = (int64_t)((deadline - now) / 1000000ull) + 1;
    }
    ctron_rt_wait_fd((int)fd, write_side, rem);
    return 1;
}

const char* ctron_net_strerror(int64_t e) {
    static CT_TLS char ct_strerr_buf[128];
#if defined(_WIN32)
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, (DWORD)e, 0, ct_strerr_buf, (DWORD)sizeof(ct_strerr_buf), NULL);
    return ct_strerr_buf;
#else
    /* macOS/BSD strerror_r 返回 char*,XSI 返回 int —— 两形都按丢弃结果处理 */
    (void)strerror_r((int)e, ct_strerr_buf, sizeof(ct_strerr_buf));
    return ct_strerr_buf;
#endif
}

/* ---- TCP ---- */

int64_t ctron_net_tcp_listen(const char* host, int64_t port, Box64* out) {
    if (CT_WSA() != 0) return -1;
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return ct_err();
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one)); /* §11.3 */
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    if (!ct_host4(host, &a.sin_addr)) {
        ct_close(fd);
        *ct_err_slot() = EINVAL;
        return -1;
    }
    if (bind(fd, (struct sockaddr*)&a, sizeof(a)) != 0) { ct_close(fd); return ct_err(); }
    if (listen(fd, 128) != 0) { ct_close(fd); return ct_err(); }
    out->v = fd;
    return 0;
}

int64_t ctron_net_tcp_sockname(int64_t fd, Box64* out) {
    struct sockaddr_in a;
    ct_socklen len = (ct_socklen)sizeof(a);
    memset(&a, 0, sizeof(a));
    if (getsockname((ct_sock)fd, (struct sockaddr*)&a, &len) != 0) return ct_err();
    out->v = (int64_t)ntohs(a.sin_port);
    return 0;
}

int64_t ctron_net_tcp_accept(int64_t lfd, Box64* out) {
    for (;;) {
        int cfd = (int)accept((ct_sock)lfd, NULL, NULL);
        if (cfd < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            return ct_err();
#endif
        }
        ct_tcp_defaults(cfd); /* §11.3 默认面在 accept 出口生效 */
        out->v = cfd;
        return 0;
    }
}

int64_t ctron_net_tcp_connect(const char* host, int64_t port, Box64* out) {
    if (CT_WSA() != 0) return -1;
    int fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return ct_err();
    ct_tcp_defaults(fd); /* §11.3 默认面在 connect 出口生效 */
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    if (!ct_host4(host, &a.sin_addr)) {
        ct_close(fd);
        *ct_err_slot() = EINVAL;
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&a, sizeof(a)) != 0) { ct_close(fd); return ct_err(); }
    out->v = fd;
    return 0;
}

/* 读:poll() 超时门(timeout_ms <= 0 = 永久阻塞)>0 n / 0 eof / <0 err(超时
 * 置 ETIMEDOUT)。字节逐条写入 int64 lane。
 * P2-C 停车点①;P3-A 探针消除:协程上下文 recv 以 MSG_DONTWAIT 直试
 * (per-call 非阻塞,fd 阻塞属性不动、P1 裸面零改)—— EAGAIN → wait_fd
 * 停车重试(deadline 收敛保 ETIMEDOUT 语义,worker 线程全程不滞留),省去
 * 每读一笔 poll(0) syscall;裸线程(未链 rt / current 为空)走下方 P1 poll
 * 门原路径,逐字节不变。 */
int64_t ctron_net_read_t(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms) {
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
    if (ct_rt_parkable()) {
        unsigned char tmp[CT_CHUNK];
        int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
        uint64_t deadline = timeout_ms > 0
            ? (uint64_t)ctron_net_now_ns() + (uint64_t)timeout_ms * 1000000ull : 0;
        for (;;) {
            ct_ssize_t n;
            n = recv((ct_sock)fd, (char*)tmp, (size_t)want, MSG_DONTWAIT);
            if (n < 0) {
                if (errno == EINTR) continue;
                if ((errno == EAGAIN || errno == EWOULDBLOCK)) { /* 未就绪:停车等就绪/超时 */
                    if (!ct_rt_park_until(fd, 0, deadline)) {
                        *ct_err_slot() = ETIMEDOUT;
                        return -1;
                    }
                    continue;                        /* 醒后重试(就绪是提示) */
                }
                return ct_err();
            }
            for (int64_t i = 0; i < (int64_t)n; i++) buf.d[i] = (int64_t)tmp[i];
            return (int64_t)n;
        }
    }
    if (timeout_ms > 0) {
        struct pollfd p;
        p.fd = (ct_sock)fd;
        p.events = POLLIN;
        p.revents = 0;
        for (;;) {
            int pr = ct_poll(&p, 1, (int)timeout_ms);
            if (pr < 0) {
#ifdef _WIN32
                return ct_err();
#else
                if (errno == EINTR) continue;
                return ct_err();
#endif
            }
            if (pr == 0) {
                *ct_err_slot() = ETIMEDOUT;
                return -1;
            }
            break;
        }
    }
    unsigned char tmp[CT_CHUNK];
    int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
    for (;;) {
        ct_ssize_t n = recv((ct_sock)fd, (char*)tmp, (size_t)want, 0);
        if (n < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            return ct_err();
#endif
        }
        for (int64_t i = 0; i < (int64_t)n; i++) buf.d[i] = (int64_t)tmp[i];
        return (int64_t)n;
    }
}

/* 写缓冲视图 lane:read_t/write 共用 —— write 逐条取 (char)lane[i]
 * P2-C 停车点②/③(write/write_str 共此发送核):EAGAIN 且协程上下文 →
 * wait_fd 等可写后重试(P1 write 为阻塞发完语义、无超时面,故永久等);
 * 裸线程 → 原 ct_err() 回退,逐字节不变。
 * P3-A:协程上下文 send 以 MSG_DONTWAIT 直试(探针本来就没有,此改与
 * read_t 对称 —— 阻塞 fd 上 send 满缓冲会滞留 worker,协程面必须 per-call
 * 非阻塞;裸面旗标零改)。 */
static int64_t ct_send_all(int64_t fd, const unsigned char* src, int64_t n) {
    int64_t off = 0;
    int nb = ct_rt_parkable();           /* 协程上下文:per-call 非阻塞直试 */
    while (off < n) {
        ct_ssize_t w = send((ct_sock)fd, (const char*)(src + off),
                            (size_t)(n - off), MSG_NOSIGNAL | (nb ? MSG_DONTWAIT : 0));
        if (w < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && nb) {
                ct_rt_park_until(fd, 1, 0);
                continue;                            /* 醒后重试(就绪是提示) */
            }
            return ct_err();
#endif
        }
        off += (int64_t)w;
    }
    return n;
}

int64_t ctron_net_write(int64_t fd, ct_view6 buf, int64_t n) {
    if (n > buf.n) n = buf.n;
    if (n <= 0) return 0;
    unsigned char tmp[CT_CHUNK];
    int64_t off = 0;
    while (off < n) {
        int64_t chunk = n - off;
        if (chunk > (int64_t)sizeof(tmp)) chunk = (int64_t)sizeof(tmp);
        for (int64_t i = 0; i < chunk; i++) tmp[i] = (unsigned char)buf.d[off + i];
        int64_t r = ct_send_all(fd, tmp, chunk);
        if (r < 0) return r;
        off += chunk;
    }
    return n;
}

int64_t ctron_net_write_str(int64_t fd, const char* s) {
    int64_t n = (int64_t)strlen(s);
    if (n <= 0) return 0;
    return ct_send_all(fd, (const unsigned char*)s, n);
}

int64_t ctron_net_close(int64_t fd) {
    int r = ct_close((ct_sock)fd);
    /* P3-A 兴趣驻留配套:close 后摘 rt 驻留登记(内核已在 close 时自动摘
     * knote/epoll 节点,钩子只清登记表;未链 rt → 弱哑元 no-op。不动 errno) */
    ctron_rt_forget_fd(fd);
    if (r != 0) return ct_err();
    return 0;
}

int64_t ctron_net_shutdown_write(int64_t fd) {
    if (shutdown((ct_sock)fd, CT_SHUT_WR) != 0) return ct_err();
    return 0;
}

int64_t ctron_net_set_nodelay(int64_t fd, int64_t on) {
    int v = on ? 1 : 0;
    if (setsockopt((ct_sock)fd, IPPROTO_TCP, TCP_NODELAY,
                   (const char*)&v, sizeof(v)) != 0) return ct_err();
    return 0;
}

int64_t ctron_net_get_nodelay(int64_t fd) {
    int v = 0;
    ct_socklen len = (ct_socklen)sizeof(v);
    if (getsockopt((ct_sock)fd, IPPROTO_TCP, TCP_NODELAY,
                   (char*)&v, &len) != 0) return ct_err();
    return (int64_t)(v != 0);
}

/* ---- UDP ---- */

int64_t ctron_net_udp_socket(Box64* out) {
    if (CT_WSA() != 0) return -1;
    int fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return ct_err();
    out->v = fd;
    return 0;
}

/* 绑定并回读实际端口(:0 语义);P1 夹具自发自收的前置 */
int64_t ctron_net_udp_bind(int64_t fd, const char* host, int64_t port, Box64* out_port) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    if (!ct_host4(host, &a.sin_addr)) {
        *ct_err_slot() = EINVAL;
        return -1;
    }
    if (bind((ct_sock)fd, (struct sockaddr*)&a, sizeof(a)) != 0) return ct_err();
    return ctron_net_tcp_sockname(fd, out_port); /* getsockname 对 UDP 同形 */
}

int64_t ctron_net_udp_sendto(int64_t fd, const char* host, int64_t port,
                             ct_view6 buf, int64_t n) {
    if (n > buf.n) n = buf.n;
    if (n <= 0) return 0;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    if (!ct_host4(host, &a.sin_addr)) {
        *ct_err_slot() = EINVAL;
        return -1;
    }
    unsigned char tmp[CT_CHUNK];
    int64_t want = n < (int64_t)sizeof(tmp) ? n : (int64_t)sizeof(tmp);
    for (int64_t i = 0; i < want; i++) tmp[i] = (unsigned char)buf.d[i];
    for (;;) {
        ct_ssize_t w = sendto((ct_sock)fd, (const char*)tmp, (size_t)want, 0,
                              (struct sockaddr*)&a, (ct_socklen)sizeof(a));
        if (w < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            return ct_err();
#endif
        }
        return (int64_t)w;
    }
}

int64_t ctron_net_udp_recvfrom(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms) {
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
    /* P2-C 停车点⑤;P3-A 探针消除:与 read_t 同形 —— MSG_DONTWAIT 直试 +
     * EAGAIN 停车重试 + deadline 收敛;裸线程走下方 P1 原路径逐字节不变。 */
    if (ct_rt_parkable()) {
        unsigned char tmp[CT_CHUNK];
        int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
        uint64_t deadline = timeout_ms > 0
            ? (uint64_t)ctron_net_now_ns() + (uint64_t)timeout_ms * 1000000ull : 0;
        for (;;) {
            ct_ssize_t n;
            n = recvfrom((ct_sock)fd, (char*)tmp, (size_t)want, MSG_DONTWAIT, NULL, NULL);
            if (n < 0) {
                if (errno == EINTR) continue;
                if ((errno == EAGAIN || errno == EWOULDBLOCK)) {
                    if (!ct_rt_park_until(fd, 0, deadline)) {
                        *ct_err_slot() = ETIMEDOUT;
                        return -1;
                    }
                    continue;
                }
                return ct_err();
            }
            for (int64_t i = 0; i < (int64_t)n; i++) buf.d[i] = (int64_t)tmp[i];
            return (int64_t)n;
        }
    }
    if (timeout_ms > 0) {
        struct pollfd p;
        p.fd = (ct_sock)fd;
        p.events = POLLIN;
        p.revents = 0;
        for (;;) {
            int pr = ct_poll(&p, 1, (int)timeout_ms);
            if (pr < 0) {
#ifdef _WIN32
                return ct_err();
#else
                if (errno == EINTR) continue;
                return ct_err();
#endif
            }
            if (pr == 0) {
                *ct_err_slot() = ETIMEDOUT;
                return -1;
            }
            break;
        }
    }
    unsigned char tmp[CT_CHUNK];
    int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
    for (;;) {
        ct_ssize_t n = recvfrom((ct_sock)fd, (char*)tmp, (size_t)want, 0, NULL, NULL);
        if (n < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            return ct_err();
#endif
        }
        for (int64_t i = 0; i < (int64_t)n; i++) buf.d[i] = (int64_t)tmp[i];
        return (int64_t)n;
    }
}

/* ---- AF_UNIX(P3-E;POSIX 专面,_WIN32 垫显式 EAFNOSUPPORT 哑元) ----
 * 与 TCP 三件的镜像点及差异:
 *   - SO_REUSEADDR 不开:AF_UNIX 无端口占用语义,路径占用不适用该旋钮;
 *   - bind 前 unlink(path) 清陈旧 socket 文件(ENOENT 常态,忽略):语义 =
 *     后绑者赢(live 监听者被夺路径后既有连接不受影响,新 connect 归新监听
 *     者;文件级残留由下一次 bind 自愈)。摘文件不进 Drop(见 std/net.ct
 *     net_unix_unlink 注),显式面交调用方;
 *   - 路径上限:sun_path 容量 darwin 104 / linux 108 字节(含 NUL)—— 取
 *     min = 104,strlen(path) >= 104 即 EINVAL(跨平台一致口径: darwin 合法
 *     路径在 linux 必合法,linux 105..107 段登记为面收缩);
 *   - accept/connect 阻塞形态与 tcp 同形(无新停车点;登记:协程上下文
 *     accept/connect 滞留 worker 与 TCP 同口径,P2-C 五停车点扩面候选 ——
 *     夹具 connect 先于 accept,backlog 承接即返不触界)。 */
#ifndef _WIN32
#define CT_UNIX_PATH_MAX 104 /* min(sizeof sun_path): darwin 104 / linux 108 */

/* 路径 → sockaddr_un(已验长);超限置 EINVAL 返 -1 */
static int ct_unix_fill(struct sockaddr_un* ua, const char* path) {
    memset(ua, 0, sizeof(*ua));
    ua->sun_family = AF_UNIX;
    if (strlen(path) >= CT_UNIX_PATH_MAX) {
        *ct_err_slot() = EINVAL;
        return -1;
    }
    strcpy(ua->sun_path, path); /* 已验长(>= 104 拒) */
    return 0;
}

int64_t ctron_net_unix_listen(const char* path, Box64* out) {
    struct sockaddr_un a;
    if (ct_unix_fill(&a, path) != 0) return -1;
    int fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return ct_err();
    (void)unlink(path); /* 陈旧 socket 文件自愈(ENOENT 常态;语义见上注) */
    if (bind(fd, (struct sockaddr*)&a,
             (ct_socklen)(offsetof(struct sockaddr_un, sun_path) + strlen(path) + 1)) != 0) {
        ct_close(fd);
        return ct_err();
    }
    if (listen(fd, 128) != 0) { ct_close(fd); return ct_err(); }
    out->v = fd;
    return 0;
}

int64_t ctron_net_unix_accept(int64_t lfd, Box64* out) {
    for (;;) {
        int cfd = (int)accept((ct_sock)lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            return ct_err();
        }
        out->v = cfd; /* 无 tcp_defaults 面(NODELAY/KEEPALIVE 均不适用 AF_UNIX) */
        return 0;
    }
}

int64_t ctron_net_unix_connect(const char* path, Box64* out) {
    struct sockaddr_un a;
    if (ct_unix_fill(&a, path) != 0) return -1;
    int fd = (int)socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return ct_err();
    if (connect(fd, (struct sockaddr*)&a,
                (ct_socklen)(offsetof(struct sockaddr_un, sun_path) + strlen(path) + 1)) != 0) {
        ct_close(fd);
        return ct_err();
    }
    out->v = fd;
    return 0;
}

int64_t ctron_net_unix_unlink(const char* path) {
    if (unlink(path) != 0) return ct_err();
    return 0;
}
#else
/* _WIN32 哑元:AF_UNIX 面 POSIX-only(登记);显式错误面即失败,不静默 */
int64_t ctron_net_unix_listen(const char* path, Box64* out) {
    (void)path; (void)out;
    *ct_err_slot() = (int64_t)WSAEAFNOSUPPORT;
    return -1;
}
int64_t ctron_net_unix_accept(int64_t lfd, Box64* out) {
    (void)lfd; (void)out;
    *ct_err_slot() = (int64_t)WSAEAFNOSUPPORT;
    return -1;
}
int64_t ctron_net_unix_connect(const char* path, Box64* out) {
    (void)path; (void)out;
    *ct_err_slot() = (int64_t)WSAEAFNOSUPPORT;
    return -1;
}
int64_t ctron_net_unix_unlink(const char* path) {
    (void)path;
    *ct_err_slot() = (int64_t)WSAEAFNOSUPPORT;
    return -1;
}
#endif

/* ---- resolve(P3-E 异步化) ---- */

/* 首个 IPv4 → 点分串入 out(≥INET_ADDRSTRLEN);返回 0 成 / 非 0 = getaddrinfo
 * rc(或 inet_ntop errno)。纯函数:不触碰 errno 槽/TLS —— 槽属执行线程,
 * 池线程与裸面共用同一实现,错误数值经返回值由调用线程落自己的槽。 */
static int64_t ct_getaddrinfo_v4(const char* host, char* out, size_t outn) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    out[0] = '\0';
    int rc = getaddrinfo(host, NULL, &hints, &res);
    if (rc != 0 || res == NULL) return (int64_t)rc;
    const void* src = &((const struct sockaddr_in*)(const void*)res->ai_addr)->sin_addr;
    if (!inet_ntop(AF_INET, src, out, (ct_socklen)outn)) {
        freeaddrinfo(res);
        return (int64_t)errno;
    }
    freeaddrinfo(res);
    return 0;
}

#ifndef _WIN32
/* ---- P3-E DNS helper 池:2 线程 + done 槽(协程面专用) ----
 * 旧实现 getaddrinfo 直接跑在调用协程所在 worker:解析期间 worker 整体滞留
 * (workers=1 时全 runtime 饿死,c_smoke T6 即此差分断言)。现形态:
 *   resolve(host) 协程内 → 组 job 槽(宿主串拷贝 + 结果槽 + done 原子旗标 +
 *   本协程 key)入 FIFO → ctron_rt_park 停车;池线程 getaddrinfo/inet_ntop 写
 *   结果槽 → release 置 done → ctron_rt_wake(key)(粘滞 pending:先唤醒后
 *   停车不丢)→ 协程醒后 acquire 轮 done,见 1 即独占读槽。
 * 槽所有权(竞态隔离三要点):
 *   1. job malloc/free 均在调用协程;池线程 release-store done 后除先取的
 *      coro_key 局部副本外绝不再触槽 —— 协程见 done(acquire)即独占,free
 *      与 wake 竞态被「先取副本后置旗标」切断;
 *   2. errno 槽属 TLS:池线程绝不写(写了是池线程的槽),rc 携带于槽内,
 *      协程醒后自行落本线程槽;
 *   3. 虚假唤醒(scope cancel_wake_all 广播、pending 残留)以 while!done
 *      再停车吸收;resolve 无超时面(签名不变)且 getaddrinfo 不可取消 ——
 *      取消广播不中断在途 resolve(登记:最长等待 = 解析本身;CI 面 host 仅
 *      localhost/数值,毫秒级)。
 * 池生命周期:首次协程 resolve 惰性起 2 线程(detached,进程生命周期常驻,
 * 无 shutdown 面);建池全败(线程创建失败)退化为调用面内联阻塞(旧语义,
 * 降级不悬挂)。裸线程面(未链 rt / current()==NULL / _WIN32)P1 原路径
 * 逐字节不变。线程预算(容量口径):workers(缺省 min(cpu,4))+ reactor 1
 * (首次 wait_fd 惰性)+ 本池 2(首次协程 resolve 惰性;不用 resolve 则零)。
 */
struct ct_dns_job {
    struct ct_dns_job* next;
    char host[256];                 /* 拷贝解耦调用方串寿命(停车期间仍有效) */
    char result[64];
    int64_t rc;                     /* 0 成(result 有效);非 0 = gai rc/errno */
    atomic_int done;                /* 池线程 release 置 1;协程 acquire 轮询 */
    void* coro_key;                 /* 提交协程 key(wake 用;见所有权要点 1) */
};

static pthread_mutex_t ct_dns_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ct_dns_cv = PTHREAD_COND_INITIALIZER;
static struct ct_dns_job* ct_dns_head = NULL;
static struct ct_dns_job** ct_dns_tail = &ct_dns_head;
static int ct_dns_up = 0;           /* 池线程 ≥1 已起(mx 内读写;线程不退 ⇒ 不可逆) */

static void* ct_dns_worker(void* arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&ct_dns_mx);
        while (ct_dns_head == NULL) pthread_cond_wait(&ct_dns_cv, &ct_dns_mx);
        struct ct_dns_job* j = ct_dns_head;
        ct_dns_head = j->next;
        if (ct_dns_head == NULL) ct_dns_tail = &ct_dns_head;
        pthread_mutex_unlock(&ct_dns_mx);

        j->rc = ct_getaddrinfo_v4(j->host, j->result, sizeof(j->result));

        void* key = j->coro_key;   /* 先取副本:done 后槽归协程(free 竞态隔离) */
        atomic_store_explicit(&j->done, 1, memory_order_release);
        ctron_rt_wake(key);
        /* j 此后绝不触碰 —— 所有权已移交唤醒协程 */
    }
}

/* 入队;返回 0 = 池承接,1 = 池不可用(建池全败),调用方内联兜底 */
static int ct_dns_submit(struct ct_dns_job* j) {
    pthread_mutex_lock(&ct_dns_mx);
    *ct_dns_tail = j;
    ct_dns_tail = &j->next;
    if (!ct_dns_up) {
        /* 起池与入队同锁:并发首提交串行化,失败路径队列恰为本 job 一个 */
        int started = 0;
        for (int i = 0; i < 2; i++) {
            pthread_t t;
            pthread_attr_t at;
            pthread_attr_init(&at);
            pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
            if (pthread_create(&t, &at, ct_dns_worker, NULL) == 0) started = 1;
            pthread_attr_destroy(&at);
        }
        ct_dns_up = started;
        if (!started) {
            ct_dns_head = NULL;
            ct_dns_tail = &ct_dns_head;
        }
    }
    pthread_cond_signal(&ct_dns_cv);
    /* 锁内取闩:解锁后 ct_dns_up 仍可被并发首提交翻转,锁外读既属数据竞争,
     * 又会让调用者误判池可用返 0,实则失败路径已内联清队 → 停车等 done 永不来。 */
    int ok = ct_dns_up;
    pthread_mutex_unlock(&ct_dns_mx);
    return ok ? 0 : 1;
}
#endif /* !_WIN32 */

/* 首个 IPv4 点分串;C-owned(thread-local 静态),Ctron 侧 str_from_c 深拷。
 * 失败返回 ""(errno 槽置 getaddrinfo rc)。
 * P3-E:协程上下文 → 入 helper 池 + park(不滞留 worker,见上注);裸线程
 * 面直接调用面阻塞(P1 原路径,结果串同槽同形)。 */
const char* ctron_net_resolve_first(const char* host) {
    static CT_TLS char ct_res_buf[64];
#ifndef _WIN32
    if (ct_rt_parkable()) {
        struct ct_dns_job* j = (struct ct_dns_job*)malloc(sizeof(*j));
        if (j == NULL) {
            *ct_err_slot() = ENOMEM;
            return "";
        }
        memset(j, 0, sizeof(*j));
        strncpy(j->host, host, sizeof(j->host) - 1);
        j->host[sizeof(j->host) - 1] = '\0';
        j->coro_key = ctron_rt_current();
        atomic_init(&j->done, 0);
        if (ct_dns_submit(j) == 0) {
            /* 停车等池;假醒(cancel 广播/pending 残留)→ 再验 done 再停 */
            while (atomic_load_explicit(&j->done, memory_order_acquire) == 0) {
                ctron_rt_park();
            }
            int64_t rc = j->rc;
            strncpy(ct_res_buf, j->result, sizeof(ct_res_buf) - 1);
            ct_res_buf[sizeof(ct_res_buf) - 1] = '\0';
            free(j);               /* done 已见 ⇒ 槽归本协程独占 */
            *ct_err_slot() = rc;   /* rc==0 时同旧路径清零错误槽 */
            if (rc != 0 || ct_res_buf[0] == '\0') return "";
            return ct_res_buf;
        }
        free(j);                   /* 池不可用:内联兜底(旧阻塞语义) */
    }
#endif
    int64_t rc = ct_getaddrinfo_v4(host, ct_res_buf, sizeof(ct_res_buf));
    *ct_err_slot() = rc;
    if (rc != 0 || ct_res_buf[0] == '\0') return "";
    return ct_res_buf;
}
