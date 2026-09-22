/*
 * ctron_dbredis.c - Ctron std/db Redis RESP2 fd 源垫片(P5-E;std/db/c_src 第三件)
 *
 * extern 面(std/db/redis.ct):
 *   int64_t ctron_dbredis_read(int64_t fd, ct_dbredis_view buf, int64_t cap)
 *     → 阻塞收 ≤ min(cap, CT_DBREDIS_CHUNK, buf.n) 字节,逐字节写 lane
 *       (read_t 约定:每条 lane 一个字节值 0..255);>0 n / 0 eof / <0 err。
 *   int64_t ctron_dbredis_write(int64_t fd, ct_dbredis_view buf, int64_t n)
 *     → 逐条取 (char)buf.d[i] 发满为止(send-all 整长 chunk 循环);n / <0。
 *   int64_t ctron_dbredis_close(int64_t fd)          → 0 成 / <0 败
 *   int64_t ctron_dbredis_last_errno(void)           → 错误槽(0 = 无错可读)
 *   int64_t ctron_dbredis_socketpair(int64_t* out)   → 夹具注入面;out 双 lane
 *     对偶 fd;POSIX 专面(_WIN32 -1)。
 *
 * 与 ctron_dbpg.c 同构镜像(符号分置:同名 extern decl 二次合并 E5030,
 * P5-D 探针实证——std/db 各驱动自有符号面前缀)。约定镜像 ctron_net.c
 * (阻塞收发;chunk 上限 4096;errno 槽 thread-local;EINTR 重试)。
 * 链接由夹具显式进行(tests/db/redis_replay/x_rd_fd emit 臂;真库冒烟
 * Task 6 nightly 同法)。interp 口径无 extern 运行时:fd 源面仅发射臂
 * 可跑(tests/net 同款)。超时/非阻塞面不做(v0:阻塞 recv)。
 */
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define CT_DBREDIS_CHUNK 4096

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define ct_dbredis_closefd(s) closesocket(s)
static __thread int ct_dbredis_errno;
static int ct_dbredis_err(void) {
    ct_dbredis_errno = (int)WSAGetLastError();
    return -1;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define ct_dbredis_closefd(s) close(s)
static __thread int ct_dbredis_errno;
static int ct_dbredis_err(void) {
    ct_dbredis_errno = errno;
    return -1;
}
#endif

/* lane 视图(发射器 &I64[] 出参形态 = (ctron_view_6){ d, n };ctron_dbpg.c
 * ct_dbpg_view 同布局镜像) */
typedef struct { int64_t* d; int64_t n; } ct_dbredis_view;

int64_t ctron_dbredis_read(int64_t fd, ct_dbredis_view buf, int64_t cap) {
    if (cap > CT_DBREDIS_CHUNK) cap = CT_DBREDIS_CHUNK;
    if (cap > buf.n) cap = buf.n;
    if (cap <= 0) return 0;
    if (buf.d == NULL) {
        ct_dbredis_errno = EFAULT;
        return -1;
    }
    unsigned char tmp[CT_DBREDIS_CHUNK];
    for (;;) {
#if defined(_WIN32)
        int n = recv((SOCKET)fd, (char*)tmp, (int)cap, 0);
#else
        ssize_t n = recv((int)fd, tmp, (size_t)cap, 0);
#endif
        if (n < 0) {
#if defined(_WIN32)
            return ct_dbredis_err();
#else
            if (errno == EINTR) continue;
            return ct_dbredis_err();
#endif
        }
        for (int64_t i = 0; i < (int64_t)n; i++) buf.d[i] = (int64_t)tmp[i];
        return (int64_t)n;
    }
}

int64_t ctron_dbredis_write(int64_t fd, ct_dbredis_view buf, int64_t n) {
    if (n <= 0) return 0;
    if (buf.d == NULL) {
        ct_dbredis_errno = EFAULT;
        return -1;
    }
    if (n > buf.n) n = buf.n;
    /* send-all 整长 chunk 循环(ct_send_all/ctron_dbpg.c 同口径) */
    unsigned char tmp[CT_DBREDIS_CHUNK];
    int64_t off = 0;
    while (off < n) {
        int64_t chunk = n - off;
        if (chunk > CT_DBREDIS_CHUNK) chunk = CT_DBREDIS_CHUNK;
        for (int64_t i = 0; i < chunk; i++) tmp[i] = (unsigned char)buf.d[off + i];
        for (;;) {
#if defined(_WIN32)
            int w = send((SOCKET)fd, (const char*)tmp, (int)chunk, 0);
#else
            ssize_t w = send((int)fd, (const char*)tmp, (size_t)chunk, MSG_NOSIGNAL);
#endif
            if (w < 0) {
#if defined(_WIN32)
                return ct_dbredis_err();
#else
                if (errno == EINTR) continue;
                return ct_dbredis_err();
#endif
            }
            off += (int64_t)w;
            break;
        }
    }
    return n;
}

int64_t ctron_dbredis_close(int64_t fd) {
    if (ct_dbredis_closefd((int)fd) != 0) return ct_dbredis_err();
    return 0;
}

int64_t ctron_dbredis_last_errno(void) {
    return (int64_t)ct_dbredis_errno;
}

/* 夹具/nightly 注入真源用:双向字节管道(_WIN32 无对应,显式 -1)。
 * out.d[0]/out.d[1] = 对偶两端(out = lane 视图)。 */
int64_t ctron_dbredis_socketpair(ct_dbredis_view out) {
#if defined(_WIN32)
    (void)out;
    ct_dbredis_errno = WSAEAFNOSUPPORT;
    return -1;
#else
    if (out.d == NULL || out.n < 2) {
        ct_dbredis_errno = EFAULT;
        return -1;
    }
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return ct_dbredis_err();
    out.d[0] = (int64_t)sv[0];
    out.d[1] = (int64_t)sv[1];
    return 0;
#endif
}
