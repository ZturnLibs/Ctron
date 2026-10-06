/* wasm_libc.c —— wasm32-unknown-unknown minilibc 本体(T37 §9.2)。
 * bare_libc.c(T40)的 wasm 移植:输出/退出面走宿主 import
 * (env.ct_print(ptr,len) / env.ct_exit(code);宿主胶水见 README.md),
 * mem/str 常规七件真实现(freestanding 代码生成会自发调用);
 * 堆 = 16MiB 线性内存静态池 bump(web 档预算,GC arena 首块 4MB 自此取材);
 * 线程/fs/dirent/dlfcn 族无定义——发射运行时引用它们的块均为 static,
 * wasm 程序不触达时被 --gc-sections 整体裁除(编译面由 include/ stub 头承接;
 * 单线程口径 = web 档 §7.8,通道/Mutex 列 T38 后)。
 * 口径:panic 面 = exit(1)(任务 longjmp 全貌列 full 档,见 include/setjmp.h)。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>

int errno;

/* ---------------- 宿主 import 面 ---------------- */
__attribute__((import_module("env"), import_name("ct_print")))
extern void ct_print(const char* ptr, unsigned len);
__attribute__((import_module("env"), import_name("ct_exit")))
extern void ct_exit(int code);

static void w_write0(const char* s) {
    const char* p = s;
    while (*p) { p++; }
    ct_print(s, (unsigned)(p - s));
}

void exit(int code) {
    ct_exit(code);
    for (;;) { __asm__ volatile("" ::: "memory"); }
}
void _exit(int code) { exit(code); }
void abort(void) { w_write0("wasm_libc: abort\n"); exit(134); }

/* ---------------- mem/str ---------------- */
void* memcpy(void* d, const void* s, size_t n) {
    unsigned char* dd = (unsigned char*)d;
    const unsigned char* ss = (const unsigned char*)s;
    while (n--) { *dd++ = *ss++; }
    return d;
}
void* memmove(void* d, const void* s, size_t n) {
    unsigned char* dd = (unsigned char*)d;
    const unsigned char* ss = (const unsigned char*)s;
    if (dd < ss) { while (n--) { *dd++ = *ss++; } }
    else { dd += n; ss += n; while (n--) { *--dd = *--ss; } }
    return d;
}
void* memset(void* d, int c, size_t n) {
    unsigned char* dd = (unsigned char*)d;
    while (n--) { *dd++ = (unsigned char)c; }
    return d;
}
int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (n--) { if (*pa != *pb) { return *pa - *pb; } pa++; pb++; }
    return 0;
}
size_t strlen(const char* s) {
    const char* p = s;
    while (*p) { p++; }
    return (size_t)(p - s);
}
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) { return 0; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* ---------------- 输出:printf 族(有界 vsnprintf 核;与 bare_libc 同文) ---------------- */
struct ct_out { char* buf; size_t cap; size_t len; };
static void out_ch(struct ct_out* o, char c) {
    if (o->buf && o->len + 1 < o->cap) { o->buf[o->len] = c; }
    o->len++;
}
static void out_str(struct ct_out* o, const char* s) {
    while (*s) { out_ch(o, *s); s++; }
}

/* %f/%g 无 libm:整数法抽十进制位(与 bare_libc 同文)。 */
static void out_g(struct ct_out* o, double v, int prec) {
    if (v != v) { out_str(o, "nan"); return; }
    if (v == v * 2.0 && v != 0.0) { out_str(o, v > 0 ? "inf" : "-inf"); return; }
    if (v < 0 || (v == 0.0 && 1.0 / v < 0)) { out_ch(o, '-'); v = -v; }
    if (v == 0.0) { out_ch(o, '0'); return; }
    int decexp = 0;
    const double P[8] = {1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128};
    const double N[8] = {1e-1, 1e-2, 1e-4, 1e-8, 1e-16, 1e-32, 1e-64, 1e-128};
    int i = 7;
    while (i >= 0) {
        if (v >= P[i]) { v *= N[i]; decexp += (1 << i); }
        i--;
    }
    i = 7;
    while (i >= 0) {
        if (v < 1.0) { v *= P[i]; decexp -= (1 << i); }
        i--;
    }
    if (v >= 10.0) { v *= 0.1; decexp++; }
    char dig[32];
    int nd = prec > 24 ? 24 : prec;
    for (i = 0; i < nd; i++) {
        dig[i] = (char)v;
        v = (v - (double)dig[i]) * 10.0;
        if (i == nd - 1 && v >= 5.0) {
            int j = i;
            while (j >= 0) {
                dig[j]++;
                if (dig[j] <= 9) { break; }
                dig[j] = 0;
                j--;
            }
            if (j < 0) { dig[0] = 1; decexp++; }
        }
    }
    if (decexp < -4 || decexp >= nd) {
        int last = nd;
        while (last > 1 && dig[last - 1] == 0) { last--; }
        out_ch(o, (char)('0' + dig[0]));
        if (last > 1) {
            out_ch(o, '.');
            for (i = 1; i < last; i++) { out_ch(o, (char)('0' + dig[i])); }
        }
        out_ch(o, 'e');
        int se = decexp >= 0 ? decexp : -decexp;
        out_ch(o, decexp >= 0 ? '+' : '-');
        if (se >= 100) { out_ch(o, (char)('0' + se / 100)); out_ch(o, (char)('0' + (se / 10) % 10)); out_ch(o, (char)('0' + se % 10)); }
        else { out_ch(o, (char)('0' + se / 10)); out_ch(o, (char)('0' + se % 10)); }
    } else {
        int intd = decexp < 0 ? 0 : decexp + 1;
        for (i = 0; i < intd; i++) { out_ch(o, (char)('0' + (i < nd ? dig[i] : 0))); }
        if (intd == 0) { out_ch(o, '0'); }
        int fs = decexp < 0 ? 0 : intd;
        int fe = nd;
        while (fe > fs && dig[fe - 1] == 0) { fe--; }
        int lead = decexp < 0 ? -decexp : 0;
        if (fe > fs) {
            out_ch(o, '.');
            int z = lead;
            while (z-- > 0) { out_ch(o, '0'); }
            for (i = fs; i < fe; i++) { out_ch(o, (char)('0' + dig[i])); }
        }
    }
}

int vsnprintf(char* buf, size_t n, const char* fmt, __builtin_va_list ap) {
    struct ct_out o; o.buf = buf; o.cap = n; o.len = 0;
    const char* p = fmt;
    while (*p) {
        if (*p != '%') { out_ch(&o, *p); p++; continue; }
        p++;
        int lcount = 0;
        while (*p == 'l') { lcount++; p++; }
        int prec = 6;
        if (*p == '.') {
            p++; prec = 0;
            while (*p >= '0' && *p <= '9') { prec = prec * 10 + (*p - '0'); p++; }
        }
        char c = *p ? *p++ : 0;
        if (c == 'd' || c == 'i') {
            long long v = lcount >= 1 ? __builtin_va_arg(ap, long long) : (long long)__builtin_va_arg(ap, int);
            char t[24]; int i = 23; t[i--] = 0;
            unsigned long long u = v < 0 ? (unsigned long long)-(v + 1) + 1ULL : (unsigned long long)v;
            if (v < 0) { out_ch(&o, '-'); }
            if (u == 0) { t[i--] = '0'; }
            while (u) { t[i--] = (char)('0' + (u % 10)); u /= 10; }
            out_str(&o, &t[i + 1]);
        } else if (c == 'u') {
            unsigned long long u = lcount >= 1 ? __builtin_va_arg(ap, unsigned long long) : (unsigned long long)__builtin_va_arg(ap, unsigned int);
            char t[24]; int i = 23; t[i--] = 0;
            if (u == 0) { t[i--] = '0'; }
            while (u) { t[i--] = (char)('0' + (u % 10)); u /= 10; }
            out_str(&o, &t[i + 1]);
        } else if (c == 'x' || c == 'X') {
            unsigned long long u = lcount >= 1 ? __builtin_va_arg(ap, unsigned long long) : (unsigned long long)__builtin_va_arg(ap, unsigned int);
            char t[20]; int i = 19; t[i--] = 0;
            if (u == 0) { t[i--] = '0'; }
            while (u) { int d4 = (int)(u & 15); t[i--] = (char)(d4 < 10 ? '0' + d4 : (c == 'x' ? 'a' : 'A') + d4 - 10); u >>= 4; }
            out_str(&o, &t[i + 1]);
        } else if (c == 's') {
            const char* s = __builtin_va_arg(ap, const char*);
            out_str(&o, s ? s : "(null)");
        } else if (c == 'c') {
            out_ch(&o, (char)__builtin_va_arg(ap, int));
        } else if (c == 'p') {
            void* pv = __builtin_va_arg(ap, void*);
            unsigned long u = (unsigned long)pv;
            out_str(&o, "0x");
            char t[12]; int i = 11; t[i--] = 0;
            if (u == 0) { t[i--] = '0'; }
            while (u) { int d4 = (int)(u & 15); t[i--] = (char)(d4 < 10 ? '0' + d4 : 'a' + d4 - 10); u >>= 4; }
            out_str(&o, &t[i + 1]);
        } else if (c == 'g' || c == 'e' || c == 'f') {
            double v = __builtin_va_arg(ap, double);
            out_g(&o, v, prec > 0 ? prec : 1);
        } else if (c == '%') {
            out_ch(&o, '%');
        } else if (c == 0) {
            break;
        } else {
            out_ch(&o, c);
        }
    }
    if (o.buf && o.cap > 0) { o.buf[o.len < o.cap ? o.len : o.cap - 1] = 0; }
    return (int)o.len;
}

int printf(const char* fmt, ...) {
    char buf[512];
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    __builtin_va_end(ap);
    w_write0(buf);
    return r;
}
int fprintf(FILE* f, const char* fmt, ...) {
    (void)f; /* web 档 stderr ≙ stdout:ct_print 单通道 */
    char buf[512];
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    __builtin_va_end(ap);
    w_write0(buf);
    return r;
}
int sprintf(char* buf, const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, (size_t)-1 / 2, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
int snprintf(char* buf, size_t n, const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
int puts(const char* s) { w_write0(s); w_write0("\n"); return 0; }
int putchar(int c) { char b[2]; b[0] = (char)c; b[1] = 0; w_write0(b); return c; }
int fflush(FILE* f) { (void)f; return 0; }

/* ---------------- setjmp 面见 include/setjmp.h ---------------- */
int _ct_setjmp(jmp_buf env) {
    (void)env;
    return 0;
}
void _ct_longjmp(jmp_buf env, int v) {
    (void)env; (void)v;
    w_write0("wasm_libc: longjmp on wasm\n");
    exit(70);
}

/* ---------------- 杂项 ---------------- */
char* getenv(const char* name) { (void)name; return 0; }

/* ---------------- 数值转换面(str→数;发射运行时 ctron_str_to_* 引用) ---------------- */
static long long ct_strtoll_core(const char* s, int base, int* ok) {
    long long v = 0;
    int neg = 0, any = 0;
    while (*s == ' ' || *s == '\t') { s++; }
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (base == 0) { base = 10; }
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') { d = *s - '0'; }
        else if (*s >= 'a' && *s <= 'z') { d = *s - 'a' + 10; }
        else if (*s >= 'A' && *s <= 'Z') { d = *s - 'A' + 10; }
        else { break; }
        if (d >= base) { break; }
        v = v * base + d;
        any = 1;
        s++;
    }
    if (ok) { *ok = any; }
    return neg ? -v : v;
}
long strtol(const char* s, char** end, int base) {
    int ok = 0;
    long long v = ct_strtoll_core(s, base, &ok);
    const char* p = s;
    while (*p == ' ' || *p == '\t') { p++; }
    if (*p == '+' || *p == '-') { p++; }
    while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) { p++; }
    if (end) { *end = (char*)(ok ? p : s); }
    return (long)v;
}
long long strtoll(const char* s, char** end, int base) {
    long v = strtol(s, end, base);
    return (long long)v;
}
int atoi(const char* s) { return (int)strtol(s, 0, 10); }
long atol(const char* s) { return strtol(s, 0, 10); }
long long atoll(const char* s) { return (long long)strtol(s, 0, 10); }
double strtod(const char* s, char** end) {
    /* 无 libm:整数位累加 + 10 的幂缩放小数/指数位 */
    double v = 0.0;
    int neg = 0, any = 0;
    const char* p = s;
    while (*p == ' ' || *p == '\t') { p++; }
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10.0 + (*p - '0'); p++; any = 1; }
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (*p >= '0' && *p <= '9') { v += (*p - '0') * scale; scale *= 0.1; p++; any = 1; }
    }
    if (any && (*p == 'e' || *p == 'E')) {
        const char* q = p + 1;
        int eneg = 0, eany = 0, ev = 0;
        if (*q == '+' || *q == '-') { eneg = (*q == '-'); q++; }
        while (*q >= '0' && *q <= '9') { ev = ev * 10 + (*q - '0'); q++; eany = 1; }
        if (eany) {
            double pw = 1.0;
            for (int i = 0; i < ev; i++) { pw *= 10.0; }
            v = eneg ? v / pw : v * pw;
            p = q;
        }
    }
    if (end) { *end = (char*)(any ? p : s); }
    return neg ? -v : v;
}

/* ---------------- 堆面:静态池 bump(无 free) ----------------
 * GC arena(ctron_amalloc)与切片字面量面自此取材;池尽 = 返回 0(响亮路径)。
 * 16MiB 计入线性内存预算(node/浏览器默认上限远大于此;无 bare 100KB 口径)。 */
static unsigned char ct_heap_pool[16 * 1024 * 1024];
static size_t ct_heap_off = 0;
void* malloc(size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (ct_heap_off + n > sizeof(ct_heap_pool)) { return 0; }
    void* p = &ct_heap_pool[ct_heap_off];
    ct_heap_off += n;
    return p;
}
void free(void* p) { (void)p; }
void* calloc(size_t a, size_t b) {
    size_t n = a * b;
    void* p = malloc(n);
    if (p) { memset(p, 0, n); }
    return p;
}
void* realloc(void* p, size_t n) {
    void* q = malloc(n);
    if (p && q) { memcpy(q, p, n); }
    return q;
}
int abs(int v) { return v < 0 ? -v : v; }
long labs(long v) { return v < 0 ? -v : v; }
long long llabs(long long v) { return v < 0 ? -v : v; }

/* stdin/stdout/stderr 描述符:仅身份面,输出族忽略流参数 */
static struct _ct_file _ct_stdin_s, _ct_stdout_s, _ct_stderr_s;
FILE* stdin = &_ct_stdin_s;
FILE* stdout = &_ct_stdout_s;
FILE* stderr = &_ct_stderr_s;

/* ---------------- sys/stat.h 桩面(fs 族编译承接;web MVP 无 fs,恒败) ---------------- */
int mkdir(const char* path, mode_t mode) {
    (void)path;
    (void)mode;
    return -1;
}
