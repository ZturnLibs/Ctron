/* tui_term.c —— pkgs/tui 系统窄桥(W3a;设计 docs/superpowers/specs/2026-10-09-tui-design.md)
 * 纯 POSIX libc(termios/poll/TIOCGWINSZ/SIGWINCH/write),零第三方;Linux/macOS 同源。
 * 纪律:C 符号一律 tui_* 前缀;胶水全局仅进程级事实(quit 旗/色档/resize 旗),
 * 应用数据不落此层(MUV 纯度,裁决 5/6)。 */

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>

static struct termios tui_saved;
static int tui_raw_on = 0;
static int tui_quit_flag = 0;
static volatile sig_atomic_t tui_resized_flag = 0;
static int tui_leave_registered = 0;

void tui_leave(void);   /* atexit 兜底前向(atexit_leave 早于定义) */

static void tui_on_winch(int sig) {
    (void)sig;
    tui_resized_flag = 1;
}

static void tui_atexit_leave(void) {
    tui_leave();
}

int tui_is_tty(void) {
    return isatty(0) ? 1 : 0;
}

void tui_enter(void) {
    if (tui_raw_on) { return; }
    struct termios t;
    if (tcgetattr(0, &tui_saved) != 0) { return; }
    t = tui_saved;
    cfmakeraw(&t);
    t.c_oflag |= OPOST;              /* 设计 §6:raw 口径保留 OPOST(\n 语义不变) */
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSAFLUSH, &t);
    tui_raw_on = 1;
    signal(SIGWINCH, tui_on_winch);
    if (!tui_leave_registered) {
        atexit(tui_atexit_leave);    /* 铁律 §8:任何退出路径必还原 */
        tui_leave_registered = 1;
    }
    /* alt screen → 隐光标 → 鼠标 → 清屏(顺序钉死) */
    write(1, "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h\x1b[2J\x1b[H", 32);
}

void tui_leave(void) {
    if (!tui_raw_on) { return; }
    /* 还原序 = 鼠标关 → 显光标 → alt screen 退 → termios 快照(设计 §8.4) */
    write(1, "\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[?1049l", 28);
    tcsetattr(0, TCSAFLUSH, &tui_saved);
    tui_raw_on = 0;
}

void tui_out(const char* s, int64_t n) {
    if (n > 0) {
        size_t done = 0;
        while (done < (size_t)n) {
            ssize_t w = write(1, s + done, (size_t)n - done);
            if (w <= 0) { break; }
            done += (size_t)w;
        }
    }
}

int64_t tui_wait(int64_t ms) {
    struct pollfd p;
    p.fd = 0;
    p.events = POLLIN;
    p.revents = 0;
    int r = poll(&p, 1, (ms < 0 ? -1 : (int)ms));
    if (r < 0) { return -1; }
    return r > 0 ? 1 : 0;
}

int64_t tui_w(void) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) != 0) { ioctl(0, TIOCGWINSZ, &ws); }
    if (ws.ws_col < 1) { return 80; }
    return (int64_t)ws.ws_col;
}

int64_t tui_h(void) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) != 0) { ioctl(0, TIOCGWINSZ, &ws); }
    if (ws.ws_row < 1) { return 24; }
    return (int64_t)ws.ws_row;
}

void tui_quit_set(void) {
    tui_quit_flag = 1;
}

int64_t tui_quit_get(void) {
    return tui_quit_flag;
}

int64_t tui_resized_take(void) {
    if (tui_resized_flag) {
        tui_resized_flag = 0;
        return 1;
    }
    return 0;
}
