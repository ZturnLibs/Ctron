/* ctron_net.c - Ctron std/net shim (posix + winsock, same facade)
 * §11.3 defaults applied at create sites (Task 6). fd travels as int64_t.
 */
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET ct_sock;
#else
#include <time.h>
#include <unistd.h>
#endif

static _Thread_local int64_t ct_net_errno_v = 0;

int64_t ctron_net_last_errno(void) { return ct_net_errno_v; }

int64_t ctron_net_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (int64_t)(t.QuadPart * 1000000000LL / f.QuadPart);
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
