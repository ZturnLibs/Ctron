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

/* W3c 解码/重放/tick 胶水态(tui_enter 早于追加块引用,故置前) */
static int ev_kind, ev_code, ev_ch, ev_shift, ev_ctrl, ev_alt, ev_mx, ev_my, ev_w2, ev_h2;
static unsigned char tui_in[64];
static int tui_in_n = 0;
static int tui_in_i = 0;
static char tui_errbuf[8192];
static int tui_tick_ms = 0;

void tui_leave(void);   /* atexit 兜底前向(atexit_leave 早于定义) */

/* write 全环(EINTR/部分写:pty 装载即 SIGWINCH,signal 无 SA_RESTART——
 * 单发 write 曾被断成 25/29 尾四字节丢失,2026-10-18 实证) */
static void tui_w1(const char* s, int n) {
    int done = 0;
    while (done < n) {
        ssize_t w = write(1, s + done, (size_t)(n - done));
        if (w <= 0) { return; }
        done += (int)w;
    }
}

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
    /* stderr 重放缓冲(§18 panic 消息重放):TUI 期全缓冲,leave 还原后冲刷 */
    setvbuf(stderr, tui_errbuf, _IOFBF, sizeof tui_errbuf);
    /* alt screen → 隐光标 → 鼠标 → 清屏(顺序钉死) */
    tui_w1("\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h\x1b[2J\x1b[H", 32);
}

void tui_leave(void) {
    if (!tui_raw_on) { return; }
    /* 还原序 = 鼠标关 → 显光标 → alt screen 退 → termios 快照(设计 §8.4) */
    tui_w1("\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[?1049l", 29);
    tcsetattr(0, TCSAFLUSH, &tui_saved);
    tui_raw_on = 0;
    /* 还原完成后再冲 stderr(panic 消息落主屏)→ 复默认无缓冲 */
    fflush(stderr);
    setvbuf(stderr, 0, _IONBF, 0);
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

/* ── W3c:输入解码状态机(设计 §6)+ stderr 重放缓冲(§18)+ tick(§18)── */

/* refill:0 超时 / 1 有数据 / -1 EOF */

static int tui_refill(int block_ms) {
    struct pollfd p;
    p.fd = 0;
    p.events = POLLIN;
    p.revents = 0;
    int r = poll(&p, 1, block_ms);
    if (r <= 0) { return 0; }
    ssize_t n = read(0, tui_in, sizeof tui_in);
    if (n <= 0) { return -1; }
    tui_in_n = (int)n;
    tui_in_i = 0;
    return 1;
}

/* 单字节取:块内环 → refill;返回 0..255 或 0 超时 / -1 EOF */
static int tui_getc(int block_ms) {
    if (tui_in_i < tui_in_n) {
        return tui_in[tui_in_i++];
    }
    int r = tui_refill(block_ms);
    if (r == 1) {
        return tui_in[tui_in_i++];
    }
    return r;
}

static void tui_set_key(int code) { ev_kind = 1; ev_code = code; }

int64_t tui_poll(int64_t ms) {
    int c = tui_getc((int)ms);
    if (c == 0) { return 0; }
    if (c < 0) { return -1; }
    ev_kind = 0; ev_code = 0; ev_ch = 0; ev_shift = 0; ev_ctrl = 0; ev_alt = 0;
    ev_mx = 0; ev_my = 0; ev_w2 = 0; ev_h2 = 0;
    if (c == 0x1b) {
        int c2 = tui_getc(25);                     /* ESC 歧义:25ms 窗 */
        if (c2 <= 0) { tui_set_key(1014); return 1; }
        if (c2 == '[') {
            int p1 = 0, p2 = 0, p3 = 0;
            int np = 0;
            int mouse = 0;
            int f = 0;
            while (1) {
                f = tui_getc(50);
                if (f <= 0) { return -1; }
                if (f == '<') { mouse = 1; np = 0; p1 = p2 = p3 = 0; continue; }
                if (f == ';') { np += 1; continue; }
                if (f >= '0' && f <= '9') {
                    if (np == 0) { p1 = p1 * 10 + (f - '0'); }
                    else { if (np == 1) { p2 = p2 * 10 + (f - '0'); } else { p3 = p3 * 10 + (f - '0'); } }
                    continue;
                }
                break;                              /* 终字 */
            }
            if (mouse) {
                if (f != 'M') { return 0; }        /* 释放 m 一期忽略 */
                int b = p1 - 32;
                if (b < 0) { b = 0; }
                if (b >= 32) { return 0; }          /* motion/滚轮登记债 */
                ev_kind = 3;
                ev_code = b % 4;
                ev_mx = p2 - 1;
                ev_my = p3 - 1;
                if (ev_mx < 0) { ev_mx = 0; }
                if (ev_my < 0) { ev_my = 0; }
                return 1;
            }
            /* 修饰键参数(p2 = 1+bitmask,无位运算:整除取模拆槽) */
            if (p2 >= 2) {
                int v = p2 - 1;
                ev_shift = v % 2;
                ev_ctrl = (v / 2) % 2;
                ev_alt = (v / 4) % 2;
            }
            if (f == 'A') { tui_set_key(1001); return 1; }
            if (f == 'B') { tui_set_key(1002); return 1; }
            if (f == 'C') { tui_set_key(1003); return 1; }
            if (f == 'D') { tui_set_key(1004); return 1; }
            if (f == 'H') { tui_set_key(1005); return 1; }
            if (f == 'F') { tui_set_key(1006); return 1; }
            if (f == '~') {
                if (p1 == 1 || p1 == 7) { tui_set_key(1005); return 1; }
                if (p1 == 4 || p1 == 8) { tui_set_key(1006); return 1; }
                if (p1 == 2) { tui_set_key(1009); return 1; }
                if (p1 == 3) { tui_set_key(1010); return 1; }
                if (p1 == 5) { tui_set_key(1007); return 1; }
                if (p1 == 6) { tui_set_key(1008); return 1; }
                return 0;
            }
            return 0;
        }
        if (c2 == 'O') {                            /* SS3 变体同映射 */
            int f = tui_getc(50);
            if (f <= 0) { return -1; }
            if (f == 'A') { tui_set_key(1001); return 1; }
            if (f == 'B') { tui_set_key(1002); return 1; }
            if (f == 'C') { tui_set_key(1003); return 1; }
            if (f == 'D') { tui_set_key(1004); return 1; }
            if (f == 'H') { tui_set_key(1005); return 1; }
            if (f == 'F') { tui_set_key(1006); return 1; }
            if (f == 'P') { tui_set_key(1015); return 1; }
            if (f == 'Q') { tui_set_key(1016); return 1; }
            if (f == 'R') { tui_set_key(1017); return 1; }
            if (f == 'S') { tui_set_key(1018); return 1; }
            return 0;
        }
        /* ESC char = Alt+char(可打印走 CHAR 面) */
        if (c2 >= 32 && c2 <= 126) {
            ev_kind = 2;
            ev_ch = c2;
            ev_alt = 1;
            return 1;
        }
        return 0;
    }
    if (c == 13) { tui_set_key(1011); return 1; }
    if (c == 9) { tui_set_key(1012); return 1; }
    if (c == 127) { tui_set_key(1013); return 1; }
    if (c >= 1 && c <= 26) { ev_kind = 1; ev_code = c; ev_ctrl = 1; return 1; }
    if (c >= 32) { ev_kind = 2; ev_ch = c; return 1; }
    return 0;                                       /* 控制杂音丢弃 */
}

int64_t tui_ev_kind(void) { return ev_kind; }
int64_t tui_ev_code(void) { return ev_code; }
int64_t tui_ev_ch(void) { return ev_ch; }
int64_t tui_ev_shift(void) { return ev_shift; }
int64_t tui_ev_ctrl(void) { return ev_ctrl; }
int64_t tui_ev_alt(void) { return ev_alt; }
int64_t tui_ev_mx(void) { return ev_mx; }
int64_t tui_ev_my(void) { return ev_my; }

/* tick 间隔(0=无;run_app wait 超时面消费) */
void tui_set_tick(int64_t ms) { tui_tick_ms = (ms > 0 ? (int)ms : 0); }
int64_t tui_tick_get(void) { return tui_tick_ms; }

/* ── W3d:色能力探测(设计 §7;启动一次存胶水 int)──
 * NO_COLOR→mono;COLORTERM truecolor/24bit→24;TERM direct→24;
 * TERM *256color→256;余→16。CTRON_TUI_COLOR 测试钩子优先(门确定性)。 */
int tui_color_level(void) {
    static int cached = 0 - 1;
    if (cached >= 0) { return cached; }
    int lvl = 1;
    const char* hook = getenv("CTRON_TUI_COLOR");
    if (hook && hook[0]) {
        if (strcmp(hook, "24") == 0) { lvl = 3; }
        else { if (strcmp(hook, "256") == 0) { lvl = 2; } else { if (strcmp(hook, "16") == 0) { lvl = 1; } else { lvl = 0; } } }
    } else {
        const char* nc = getenv("NO_COLOR");
        if (nc && nc[0]) {
            lvl = 0;
        } else {
            const char* ct = getenv("COLORTERM");
            if (ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"))) {
                lvl = 3;
            } else {
                const char* tm = getenv("TERM");
                if (tm && strstr(tm, "direct")) { lvl = 3; }
                else { if (tm && strstr(tm, "256color")) { lvl = 2; } else { lvl = 1; } }
            }
        }
    }
    cached = lvl;
    return lvl;
}
