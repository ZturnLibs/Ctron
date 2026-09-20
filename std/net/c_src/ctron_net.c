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
 */
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
typedef int ct_sock;
typedef socklen_t ct_socklen;
typedef ssize_t ct_ssize_t;
#define CT_TLS _Thread_local
#define ct_close(fd) close(fd)
#define ct_poll poll
#define CT_SHUT_WR SHUT_WR
#endif

static CT_TLS int64_t ct_net_errno_v = 0;

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
 * _WIN32 无 rt(POSIX-only),停车面整体裁掉,P1 行为不变。 */
#if !defined(_WIN32)
__attribute__((weak)) void ctron_rt_wait_fd(int fd, int write_side, int64_t timeout_ms) {
    (void)fd; (void)write_side; (void)timeout_ms;
}
__attribute__((weak)) void* ctron_rt_current(void) { return 0; }
__attribute__((weak)) void ctron_rt_sleep_ms(int64_t ms) { (void)ms; }
#endif

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
 * P2-C 停车点①:协程上下文走专用路径(0 超时 poll 探针 + EAGAIN → wait_fd
 * 停车重试,deadline 收敛保 ETIMEDOUT 语义,worker 线程全程不滞留);裸线程
 * (未链 rt / current 为空)走下方 P1 poll 门原路径,逐字节不变。 */
int64_t ctron_net_read_t(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms) {
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
    if (ct_rt_parkable()) {
        unsigned char tmp[CT_CHUNK];
        int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
        uint64_t deadline = timeout_ms > 0
            ? (uint64_t)ctron_net_now_ns() + (uint64_t)timeout_ms * 1000000ull : 0;
        for (;;) {
            struct pollfd p;
            ct_ssize_t n;
            int pr;
            memset(&p, 0, sizeof p);
            p.fd = (ct_sock)fd;
            p.events = POLLIN;
            pr = ct_poll(&p, 1, 0);                  /* 非阻塞探针:线程不滞留 */
            if (pr < 0) {
                if (errno == EINTR) continue;
                return ct_err();
            }
            if (pr == 0) {                           /* 未就绪:停车等就绪/超时 */
                if (!ct_rt_park_until(fd, 0, deadline)) {
                    *ct_err_slot() = ETIMEDOUT;
                    return -1;
                }
                continue;                            /* 醒后重探(就绪是提示) */
            }
            n = recv((ct_sock)fd, (char*)tmp, (size_t)want, 0);
            if (n < 0) {
                if (errno == EINTR) continue;
                if ((errno == EAGAIN || errno == EWOULDBLOCK)) { /* 假就绪竞态:停车重试 */
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
 * 裸线程 → 原 ct_err() 回退,逐字节不变。 */
static int64_t ct_send_all(int64_t fd, const unsigned char* src, int64_t n) {
    int64_t off = 0;
    while (off < n) {
        ct_ssize_t w = send((ct_sock)fd, (const char*)(src + off),
                            (size_t)(n - off), MSG_NOSIGNAL);
        if (w < 0) {
#ifdef _WIN32
            return ct_err();
#else
            if (errno == EINTR) continue;
            if ((errno == EAGAIN || errno == EWOULDBLOCK) && ct_rt_parkable()) {
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
    if (ct_close((ct_sock)fd) != 0) return ct_err();
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
    /* P2-C 停车点⑤:协程上下文专用路径(与 read_t 同形:0 超时探针 + EAGAIN
     * 停车重试 + deadline 收敛);裸线程走下方 P1 原路径逐字节不变。 */
    if (ct_rt_parkable()) {
        unsigned char tmp[CT_CHUNK];
        int64_t want = cap < (int64_t)sizeof(tmp) ? cap : (int64_t)sizeof(tmp);
        uint64_t deadline = timeout_ms > 0
            ? (uint64_t)ctron_net_now_ns() + (uint64_t)timeout_ms * 1000000ull : 0;
        for (;;) {
            struct pollfd p;
            ct_ssize_t n;
            int pr;
            memset(&p, 0, sizeof p);
            p.fd = (ct_sock)fd;
            p.events = POLLIN;
            pr = ct_poll(&p, 1, 0);
            if (pr < 0) {
                if (errno == EINTR) continue;
                return ct_err();
            }
            if (pr == 0) {
                if (!ct_rt_park_until(fd, 0, deadline)) {
                    *ct_err_slot() = ETIMEDOUT;
                    return -1;
                }
                continue;
            }
            n = recvfrom((ct_sock)fd, (char*)tmp, (size_t)want, 0, NULL, NULL);
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

/* ---- resolve ---- */

/* 首个 IPv4 点分串;C-owned(thread-local 静态),Ctron 侧 str_from_c 深拷。
 * 失败返回 ""(errno 槽置 getaddrinfo rc)。 */
const char* ctron_net_resolve_first(const char* host) {
    static CT_TLS char ct_res_buf[64];
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, NULL, &hints, &res);
    if (rc != 0 || res == NULL) {
        *ct_err_slot() = rc;
        return "";
    }
    const void* src = &((const struct sockaddr_in*)(const void*)res->ai_addr)->sin_addr;
    if (!inet_ntop(AF_INET, src, ct_res_buf, (ct_socklen)sizeof(ct_res_buf))) {
        freeaddrinfo(res);
        *ct_err_slot() = errno;
        return "";
    }
    freeaddrinfo(res);
    return ct_res_buf;
}
