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

int64_t ctron_net_last_errno(void) { return ct_net_errno_v; }

#ifdef _WIN32
#define ct_err() (ct_net_errno_v = (int64_t)WSAGetLastError(), -1)
#else
static int64_t ct_err(void) { ct_net_errno_v = (int64_t)errno; return -1; }
#endif

/* Box64 镜像 Ctron struct Box64 { var v: I64 }(声明序 = C 声明序,单 I64 字段;
 * 独立 TU 布局同型即 ABI 兼容 —— tests/ffi/repr_c 先例) */
typedef struct { int64_t v; } Box64;

/* I64 视图 lane 镜像:&I64[] 发射 ctron_view_6 { d, n } 按值(§9.6) */
typedef struct { int64_t* d; int64_t n; } ct_view6;

/* 视图 lane 缓冲一次搬运上限(栈上暂存;超过部分由调用方分次读写) */
#define CT_CHUNK 4096

#ifdef _WIN32
static int ct_wsa_once(void) {
    static WSADATA wsa;
    static int wsa_ok = 0;
    if (!wsa_ok) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            ct_net_errno_v = (int64_t)WSAGetLastError();
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
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { ct_net_errno_v = errno; return -1; }
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

void ctron_net_sleep_ms(int64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = (time_t)(ms / 1000);
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) { }
#endif
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
        ct_net_errno_v = EINVAL;
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
        ct_net_errno_v = EINVAL;
        return -1;
    }
    if (connect(fd, (struct sockaddr*)&a, sizeof(a)) != 0) { ct_close(fd); return ct_err(); }
    out->v = fd;
    return 0;
}

/* 读:poll() 超时门(timeout_ms <= 0 = 永久阻塞)>0 n / 0 eof / <0 err(超时
 * 置 ETIMEDOUT)。字节逐条写入 int64 lane。 */
int64_t ctron_net_read_t(int64_t fd, ct_view6 buf, int64_t cap, int64_t timeout_ms) {
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
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
                ct_net_errno_v = ETIMEDOUT;
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

/* 写缓冲视图 lane:read_t/write 共用 —— write 逐条取 (char)lane[i] */
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
        ct_net_errno_v = EINVAL;
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
        ct_net_errno_v = EINVAL;
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
                ct_net_errno_v = ETIMEDOUT;
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
        ct_net_errno_v = rc;
        return "";
    }
    const void* src = &((const struct sockaddr_in*)(const void*)res->ai_addr)->sin_addr;
    if (!inet_ntop(AF_INET, src, ct_res_buf, (ct_socklen)sizeof(ct_res_buf))) {
        freeaddrinfo(res);
        ct_net_errno_v = errno;
        return "";
    }
    freeaddrinfo(res);
    return ct_res_buf;
}
