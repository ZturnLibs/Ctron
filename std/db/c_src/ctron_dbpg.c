/*
 * ctron_dbpg.c - Ctron std/db PostgreSQL 线协议 fd 源垫片(P5-C;std/db/c_src 第二件)
 *
 * extern 面(std/db/pg.ct):
 *   int64_t ctron_dbpg_read(int64_t fd, int64_t* buf, int64_t cap)
 *     → 阻塞收 ≤ min(cap, CT_DBPG_CHUNK) 字节,逐字节写 lane(read_t 约定:
 *       每条 lane 一个字节值 0..255);>0 n / 0 eof / <0 err。
 *   int64_t ctron_dbpg_write(int64_t fd, const int64_t* buf, int64_t n)
 *     → 逐条取 (char)lane[i] 发满为止(send-all);n / <0 err。
 *   int64_t ctron_dbpg_close(int64_t fd)          → 0 成 / <0 败
 *   int64_t ctron_dbpg_last_errno(void)           → 错误槽(0 = 无错可读)
 *
 * 约定镜像 ctron_net.c(阻塞收发;chunk 上限 4096;errno 槽 thread-local;
 * EINTR 重试)。与 net 垫片分置的理由:std/db 不得 use std.net(Global
 * Constraints 互斥树),fd 源自有符号面前缀 ctron_dbpg_*,链接由夹具显式
 * 进行(tests/db/run.sh x_fd_edge emit 臂;真库冒烟 Task 6 nightly 同法)。
 * interp 口径无 extern 运行时:fd 源面仅发射臂可跑(tests/net 同款)。
 * 超时/非阻塞面不做(v0:阻塞 recv;Task 6 nightly 依真库形态再演进)。
 */
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define CT_DBPG_CHUNK 4096

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define ct_dbpg_closefd(s) closesocket(s)
static __thread int ct_dbpg_errno;
static int ct_dbpg_err(void) {
    ct_dbpg_errno = (int)WSAGetLastError();
    return -1;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define ct_dbpg_closefd(s) close(s)
static __thread int ct_dbpg_errno;
static int ct_dbpg_err(void) {
    ct_dbpg_errno = errno;
    return -1;
}
#endif

int64_t ctron_dbpg_read(int64_t fd, int64_t* buf, int64_t cap) {
    if (cap > CT_DBPG_CHUNK) cap = CT_DBPG_CHUNK;
    if (cap <= 0) return 0;
    if (buf == NULL) {
        ct_dbpg_errno = EFAULT;
        return -1;
    }
    unsigned char tmp[CT_DBPG_CHUNK];
    for (;;) {
#if defined(_WIN32)
        int n = recv((SOCKET)fd, (char*)tmp, (int)cap, 0);
#else
        ssize_t n = recv((int)fd, tmp, (size_t)cap, 0);
#endif
        if (n < 0) {
#if defined(_WIN32)
            return ct_dbpg_err();
#else
            if (errno == EINTR) continue;
            return ct_dbpg_err();
#endif
        }
        for (int64_t i = 0; i < (int64_t)n; i++) buf[i] = (int64_t)tmp[i];
        return (int64_t)n;
    }
}

int64_t ctron_dbpg_write(int64_t fd, const int64_t* buf, int64_t n) {
    if (n > CT_DBPG_CHUNK) n = CT_DBPG_CHUNK; /* pg_send_msg_fd 暂存 lane 上限同源 */
    if (n <= 0) return 0;
    if (buf == NULL) {
        ct_dbpg_errno = EFAULT;
        return -1;
    }
    unsigned char tmp[CT_DBPG_CHUNK];
    for (int64_t i = 0; i < n; i++) tmp[i] = (unsigned char)buf[i];
    int64_t off = 0;
    while (off < n) {
#if defined(_WIN32)
        int w = send((SOCKET)fd, (const char*)(tmp + off), (int)(n - off), 0);
#else
        ssize_t w = send((int)fd, (const char*)(tmp + off), (size_t)(n - off), MSG_NOSIGNAL);
#endif
        if (w < 0) {
#if defined(_WIN32)
            return ct_dbpg_err();
#else
            if (errno == EINTR) continue;
            return ct_dbpg_err();
#endif
        }
        off += (int64_t)w;
    }
    return n;
}

int64_t ctron_dbpg_close(int64_t fd) {
    if (ct_dbpg_closefd((int)fd) != 0) return ct_dbpg_err();
    return 0;
}

int64_t ctron_dbpg_last_errno(void) {
    return (int64_t)ct_dbpg_errno;
}
