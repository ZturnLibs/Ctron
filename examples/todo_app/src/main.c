#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>
#include <setjmp.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <errno.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__linux__)
#include <unistd.h>
#endif
static char* ctron_abase = 0;
static size_t ctron_aoff = 0;
static size_t ctron_acap = 0;
static void* ctron_amalloc(size_t n) { n = (n + 15) & ~(size_t)15; if (ctron_aoff + n > ctron_acap) { size_t nc = ctron_acap ? ctron_acap * 2 : (size_t)4194304; while (nc < ctron_aoff + n) { nc = nc * 2; } ctron_abase = (char*)malloc(nc); ctron_aoff = 0; ctron_acap = nc; } void* p = ctron_abase + ctron_aoff; ctron_aoff += n; return p; }
typedef struct { unsigned long long magic; char** items; int n; int cap; } ctron_list;
typedef struct { int32_t v; } ctron_cell;
static ctron_list* ctron_list_new(void) { ctron_list* l = (ctron_list*)ctron_amalloc(sizeof(ctron_list)); l->magic = 0x4354726F6E4C7374ULL; l->items = 0; l->n = 0; l->cap = 0; return l; }
static int ctron_len(const void* p) { const char* s = (const char*)p; unsigned long long m = 0; if (strlen(s) >= 8) { memcpy(&m, s, 8); if (m == 0x4354726F6E4C7374ULL) { return ((const ctron_list*)p)->n; } } return (int)strlen(s); }
static void ctron_list_push(ctron_list* l, const char* s) { if (l->n == l->cap) { int nc = l->cap ? l->cap * 2 : 8; char** ni = (char**)ctron_amalloc(sizeof(char*) * (size_t)nc); int q = 0; while (q < l->n) { ni[q] = l->items[q]; q += 1; } l->items = ni; l->cap = nc; } l->items[l->n] = (char*)s; l->n += 1; }
static ctron_cell* ctron_cell_new(int32_t v0) { ctron_cell* c = (ctron_cell*)ctron_amalloc(sizeof(ctron_cell)); c->v = v0; return c; }
static void ctron_print_i32(int32_t v) { printf("%d\n", v); }
static const char* ctron_str_concat(const char* a, const char* b) { size_t la = strlen(a), lb = strlen(b); char* r = (char*)ctron_amalloc(la + lb + 1); memcpy(r, a, la); memcpy(r + la, b, lb); r[la + lb] = 0; return r; }
static const char* ctron_i32_to_string(int32_t v) { char* r = (char*)ctron_amalloc(16); snprintf(r, 16, "%d", v); return r; }
static void ctron_print_i64(int64_t v) { printf("%lld\n", (long long)v); }
static int ctron_byte_at(const char* s, int i) { return (int)(unsigned char)s[i]; }
static const char* ctron_utf8_enc(int cp) { static const char* rep = "\xEF\xBF\xBD"; unsigned char b[5]; int un = 0; if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { return rep; } if (cp < 0x80) { b[un++] = (unsigned char)cp; } else if (cp < 0x800) { b[un++] = (unsigned char)(0xC0 | (cp >> 6)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } else if (cp < 0x10000) { b[un++] = (unsigned char)(0xE0 | (cp >> 12)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } else { b[un++] = (unsigned char)(0xF0 | (cp >> 18)); b[un++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } b[un] = 0; char* r = (char*)ctron_amalloc((size_t)un + 1); memcpy(r, b, (size_t)un); r[un] = 0; return r; }
static const char* ctron_byte_slice(const char* s, int a, int b) { int n = b - a; char* r = (char*)ctron_amalloc((size_t)n + 1); memcpy(r, s + a, (size_t)n); r[n] = 0; return r; }
static const char* ctron_str_from_c(const char* p) { size_t n = strlen(p); char* r = (char*)ctron_amalloc(n + 1); memcpy(r, p, n + 1); return r; }
static const char* ctron_read_file(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return ""; } fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char* r = (char*)ctron_amalloc((size_t)n + 1); size_t rd = fread(r, 1, (size_t)n, f); r[rd] = 0; fclose(f); return r; }
static const char* ctron_read_dir(const char* path) { DIR* d = opendir(path); if (!d) { return NULL; } size_t cap = 256; size_t len = 0; char* r = (char*)malloc(cap); r[0] = 0; struct dirent* de; while ((de = readdir(d)) != NULL) { if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) { continue; } size_t bl = strlen(de->d_name); while (len + bl + 2 > cap) { cap *= 2; } char* nb = (char*)realloc(r, cap); r = nb; memcpy(r + len, de->d_name, bl); len += bl; r[len] = 10; len += 1; } closedir(d); r[len] = 0; return r; }
static int ctron_fs_exists(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return 0; } fclose(f); return 1; }
static int ctron_fs_write(const char* path, const char* data) { FILE* f = fopen(path, "wb"); if (!f) { return 0; } size_t n = strlen(data); size_t w = fwrite(data, 1, n, f); fclose(f); return w == n; }
static int ctron_fs_delete(const char* path) { return remove(path) == 0; }
static int64_t ctron_now_ms(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL; }
static const char* ctron_now_ms_text(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); long long ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL; char* r = (char*)ctron_amalloc(32); snprintf(r, 32, "%lld", ms); return r; }
typedef int64_t ct_i;
typedef struct { int variant; ct_i v; } ct_res;
typedef struct ct_scope { int cancelled; pthread_mutex_t mu; } ct_scope;
static ct_scope* ct_scope_new(void) { ct_scope* s = (ct_scope*)calloc(1, sizeof(ct_scope)); pthread_mutex_init(&s->mu, 0); return s; }
static pthread_mutex_t ct_gm = PTHREAD_MUTEX_INITIALIZER;
static void ct_glock(void) { pthread_mutex_lock(&ct_gm); }
static void ct_gunlock(void) { pthread_mutex_unlock(&ct_gm); }
typedef struct ct_task { pthread_t th; void* env; void* (*shim)(void*); ct_i result; int state; char pmsg[128]; ct_scope* scope; jmp_buf jmp; } ct_task;
static __thread ct_task* ct_tls_task = 0;
__attribute__((noinline)) static ct_task** ct_tls_slot(void) { return &ct_tls_task; }
typedef struct ct_chan { pthread_mutex_t mu; pthread_cond_t cv_full, cv_empty; ct_i buf[64]; int head, tail, cnt, cap; } ct_chan;
static ct_chan* ct_chans[64]; static int ct_nchans = 0;
__attribute__((weak)) int ctron_rt_active(void) { return 0; }
__attribute__((weak)) void ctron_rt_init(int workers) { (void)workers; }
__attribute__((weak)) void* ctron_rt_run(void (*fn)(void*), void* arg, void* key) { (void)fn; (void)arg; (void)key; return 0; }
__attribute__((weak)) void ctron_rt_join_key(void* key) { (void)key; }
__attribute__((weak)) void ctron_rt_park(void) { }
__attribute__((weak)) void ctron_rt_wake(void* key) { (void)key; }
__attribute__((weak)) void ctron_rt_notify_done(void* key) { (void)key; }
__attribute__((weak)) void ctron_rt_cancel_wake_all(void) { }
__attribute__((weak)) void* ctron_rt_current(void) { return 0; }
static int ct_rt_active(void) {
#if !defined(_WIN32)
    return ctron_rt_active();
#else
    return 0;
#endif
}
static int ct_rt_coro_ctx(void) {
#if !defined(_WIN32)
    return ctron_rt_active() && ctron_rt_current() != 0;
#else
    return 0;
#endif
}
static int ct_rt_env_coro(void) { const char* e = getenv("CTRON_RT"); return e != 0 && strcmp(e, "coro") == 0; }
static ct_task* ct_task_self(void) {
#if !defined(_WIN32)
    if (ctron_rt_active()) { void* _k = ctron_rt_current(); if (_k) { return (ct_task*)_k; } }
#endif
    return *ct_tls_slot();
}
static void ct_cancel_broadcast(void) { int i; for (i = 0; i < ct_nchans; i++) { pthread_mutex_lock(&ct_chans[i]->mu); pthread_cond_broadcast(&ct_chans[i]->cv_full); pthread_cond_broadcast(&ct_chans[i]->cv_empty); pthread_mutex_unlock(&ct_chans[i]->mu); } if (ct_rt_active()) ctron_rt_cancel_wake_all(); }
typedef struct { void (*fn)(void*); void* obj; } ct_drent;
static ct_drent ct_drstack[2048];
static int ct_drn = 0;
static void ctron_drop_push(void (*fn)(void*), void* obj) { if (ct_drn < 2048) { ct_drstack[ct_drn].fn = fn; ct_drstack[ct_drn].obj = obj; ct_drn += 1; } }
static void ctron_drop_pop(void) { if (ct_drn > 0) { ct_drn -= 1; } }
static void ctron_drop_unwind(void) { while (ct_drn > 0) { ct_drn -= 1; ct_drstack[ct_drn].fn(ct_drstack[ct_drn].obj); } }
static int ctron_cb_depth = 0;
static void ctron_panic(const char* m) { fflush(stdout); fprintf(stderr, "%s\n", m); ctron_drop_unwind(); if (ctron_cb_depth > 0) { exit(1); } ct_task* _tp = ct_task_self(); if (_tp) { snprintf(_tp->pmsg, 128, "%s", m); _tp->state = 2; if (_tp->scope) { _tp->scope->cancelled = 1; } ct_cancel_broadcast(); longjmp(_tp->jmp, 1); } exit(1); }
static int64_t ctron_i64_add(int64_t a, int64_t b) { int64_t r; if (__builtin_add_overflow(a, b, &r)) { ctron_panic("integer overflow (+)"); } return r; }
static int64_t ctron_i64_sub(int64_t a, int64_t b) { int64_t r; if (__builtin_sub_overflow(a, b, &r)) { ctron_panic("integer overflow (-)"); } return r; }
static int64_t ctron_i64_mul(int64_t a, int64_t b) { int64_t r; if (__builtin_mul_overflow(a, b, &r)) { ctron_panic("integer overflow (*)"); } return r; }
static int64_t ctron_i64_div(int64_t a, int64_t b) { if (b == 0) { ctron_panic("division by zero"); } if (b == -1 && a == (-9223372036854775807LL - 1)) { ctron_panic("integer overflow (/)"); } return a / b; }
static int64_t ctron_i64_mod(int64_t a, int64_t b) { if (b == 0) { ctron_panic("division by zero"); } if (b == -1 && a == (-9223372036854775807LL - 1)) { ctron_panic("integer overflow (%)"); } return a % b; }
static int64_t ctron_i64_neg(int64_t a) { int64_t r; if (__builtin_sub_overflow(0, a, &r)) { ctron_panic("integer overflow (-)"); } return r; }
static int32_t ctron_w_add(int32_t a, int32_t b, int32_t lo, int32_t hi) { int32_t r = a + b; if (r < lo || r > hi) { ctron_panic("integer overflow (+)"); } return r; }
static int32_t ctron_w_sub(int32_t a, int32_t b, int32_t lo, int32_t hi) { int32_t r = a - b; if (r < lo || r > hi) { ctron_panic("integer overflow (-)"); } return r; }
static int32_t ctron_w_mul(int32_t a, int32_t b, int32_t lo, int32_t hi) { int32_t r = a * b; if (r < lo || r > hi) { ctron_panic("integer overflow (*)"); } return r; }
static int32_t ctron_w_div(int32_t a, int32_t b) { if (b == 0) { ctron_panic("division by zero"); } return a / b; }
static int32_t ctron_w_mod(int32_t a, int32_t b) { if (b == 0) { ctron_panic("division by zero"); } return a % b; }
static uint64_t ctron_u64_add(uint64_t a, uint64_t b) { uint64_t r; if (__builtin_add_overflow(a, b, &r)) { ctron_panic("integer overflow (+)"); } return r; }
static uint64_t ctron_u64_sub(uint64_t a, uint64_t b) { uint64_t r; if (__builtin_sub_overflow(a, b, &r)) { ctron_panic("integer overflow (-)"); } return r; }
static uint64_t ctron_u64_mul(uint64_t a, uint64_t b) { uint64_t r; if (__builtin_mul_overflow(a, b, &r)) { ctron_panic("integer overflow (*)"); } return r; }
static uint64_t ctron_u64_div(uint64_t a, uint64_t b) { if (b == 0) { ctron_panic("division by zero"); } return a / b; }
static uint64_t ctron_u64_mod(uint64_t a, uint64_t b) { if (b == 0) { ctron_panic("division by zero"); } return a % b; }
static const char* ctron_i64_to_string(int64_t v) { char* r = (char*)ctron_amalloc(32); snprintf(r, 32, "%lld", (long long)v); return r; }
static void* ct_shim_tramp(void* p) { ct_task* t = (ct_task*)p; *ct_tls_slot() = t; if (setjmp(t->jmp) == 0) { t->shim(t->env); t->state = 1; } if (ct_rt_active()) ctron_rt_notify_done((void*)t); return 0; }
static ct_task* ct_spawn(void* (*shim)(void*), void* env, ct_scope* sc) { ct_task* t = (ct_task*)calloc(1, sizeof(ct_task)); t->shim = shim; t->env = env; t->scope = sc; if (!ct_rt_active() && ct_rt_env_coro()) ctron_rt_init(0); if (ct_rt_active()) { ctron_rt_run((void (*)(void*))ct_shim_tramp, t, (void*)t); return t; } pthread_create(&t->th, 0, ct_shim_tramp, t); return t; }
static ct_i ct_join(ct_task* t) { void* r; if (ct_rt_active()) { ctron_rt_join_key((void*)t); } else { pthread_join(t->th, &r); } if (t->state == 2) { ctron_panic(t->pmsg); } return t->result; }
static ct_res ct_join_or(ct_task* t) { void* r; if (ct_rt_active()) { ctron_rt_join_key((void*)t); } else { pthread_join(t->th, &r); } ct_res res; if (t->state == 2) { res.variant = 1; res.v = (ct_i)(long)"task panic"; } else { res.variant = 0; res.v = t->result; } return res; }
static ct_chan* ct_ch_make(int cap) { if (cap <= 0 || cap > 64) { cap = 64; } ct_chan* c = (ct_chan*)calloc(1, sizeof(ct_chan)); pthread_mutex_init(&c->mu, 0); pthread_cond_init(&c->cv_full, 0); pthread_cond_init(&c->cv_empty, 0); c->cap = cap; if (ct_nchans < 64) { ct_chans[ct_nchans] = c; ct_nchans += 1; } return c; }
static ct_res ct_ch_send(ct_chan* c, ct_i v) { ct_res res; pthread_mutex_lock(&c->mu); while (c->cnt == c->cap) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_full, &c->mu); } c->buf[c->tail] = v; c->tail = (c->tail + 1) % 64; c->cnt += 1; pthread_cond_broadcast(&c->cv_empty); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); res.variant = 0; res.v = 0; return res; }
static ct_res ct_ch_recv(ct_chan* c) { ct_res res; pthread_mutex_lock(&c->mu); while (c->cnt == 0) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_empty, &c->mu); } res.variant = 0; res.v = c->buf[c->head]; c->head = (c->head + 1) % 64; c->cnt -= 1; pthread_cond_broadcast(&c->cv_full); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); return res; }
typedef ct_i (*ct_fn0)(void);
typedef ct_i (*ct_fn1)(ct_i);
typedef ct_i (*ct_fn2)(ct_i, ct_i);
typedef ct_i (*ct_fn3)(ct_i, ct_i, ct_i);
typedef struct { void* fn; void* env; } ct_clo;
typedef ct_clo* ct_clop;
typedef ct_i (*ct_cfn0)(void*);
typedef ct_i (*ct_cfn1)(void*, ct_i);
typedef ct_i (*ct_cfn2)(void*, ct_i, ct_i);
typedef ct_i (*ct_cfn3)(void*, ct_i, ct_i, ct_i);
static ct_i ctron_clo_tramp2(ct_i _e, void* _u) { ct_clop _b = (ct_clop)_u; return ((ct_cfn1)(_b->fn))(_b->env, _e); }
static ct_i ctron_clo_tramp3(ct_i _e, ct_i _x, void* _u) { ct_clop _b = (ct_clop)_u; return ((ct_cfn2)(_b->fn))(_b->env, _e, _x); }
static ctron_list* ct_parallel_mapL(ctron_list* a, ct_fn1 f) { ctron_list* r = ctron_list_new(); int i; for (i = 0; i < a->n; i++) { ctron_list_push(r, (const char*)(long)f((ct_i)(long)a->items[i])); } return r; }
static ct_i ct_parallel_reduceL(ctron_list* a, ct_i init, ct_fn2 f) { ct_i acc = init; int i; for (i = 0; i < a->n; i++) { acc = f(acc, (ct_i)(long)a->items[i]); } return acc; }
static const char* ctron_cli_input = NULL;
static const char* ctron_entry(void) { return ctron_cli_input ? ctron_cli_input : ""; }
static const char* ctron_anchor = "";
static const char* ctron_embed_src = "";
static const char* ctron_embedded(void) { return ctron_embed_src; }
static const char* ctron_dom_title_buf = "";
static void ctron_dom_set_title(const char* s) { ctron_dom_title_buf = s; }
static const char* ctron_dom_title(void) { return ctron_dom_title_buf; }
static int32_t ctron_char_len(const char* s) { int32_t n = 0; while (*s) { if ((*s & 0xC0) != 0x80) { n += 1; } s += 1; } return n; }
static const char* ctron_read_file_cli(const char* p) { if (ctron_cli_input) { return ctron_read_file(ctron_cli_input); } return ctron_read_file(p); }
#if !defined(_WIN32)
static long ctron_ext_dispatch0(const char* nm, int n, const char** fr) {
    void* p; long u[12]; int i; int k = 0;
    for (i = 0; i < n; i++) {
        if (!fr[i] || !fr[i][0]) { break; }
        if (fr[i][1] != ':') { ctron_panic("extern 帧损坏"); }
        if (getenv("CTRON_GUI_TRACE") != 0) { fprintf(stderr, "FR[%d]=%s\n", i, fr[i] ? fr[i] : "(null)"); }
        if (fr[i][0] == 'i') { u[k++] = strtol(fr[i] + 2, 0, 10); } else { if (fr[i][0] == 's') { u[k++] = (long)(fr[i] + 2); } else { ctron_panic("extern 解释口径:不支持参数类型"); } }
    }
    if (getenv("CTRON_GUI_TRACE") != 0) { fprintf(stderr, "DISPATCH nm=%s n=%d k=%d\n", nm, n, k); }
    p = dlsym(RTLD_DEFAULT, nm);
    if (!p) { ctron_panic("extern 符号未找到(解释口径:符号须链接进解释器宿主)"); }
    switch (k) {
        case 0: return (long)((long (*)(void))p)();
        case 1: return ((long (*)(long))p)(u[0]);
        case 2: return ((long (*)(long, long))p)(u[0], u[1]);
        case 3: return ((long (*)(long, long, long))p)(u[0], u[1], u[2]);
        case 4: return ((long (*)(long, long, long, long))p)(u[0], u[1], u[2], u[3]);
        case 5: return ((long (*)(long, long, long, long, long))p)(u[0], u[1], u[2], u[3], u[4]);
        case 6: return ((long (*)(long, long, long, long, long, long))p)(u[0], u[1], u[2], u[3], u[4], u[5]);
        case 7: return ((long (*)(long, long, long, long, long, long, long))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6]);
        case 8: return ((long (*)(long, long, long, long, long, long, long, long))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
        case 9: return ((long (*)(int, int, int, int, int, int, int, int, int))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8]);
        case 10: return ((long (*)(int, int, int, int, int, int, int, int, int, int))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9]);
        case 11: return ((long (*)(int, int, int, int, int, int, int, int, int, int, int))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10]);
        default: return ((long (*)(int, int, int, int, int, int, int, int, int, int, int, int))p)(u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11]);
    }
}
int ctron_ext_dispatch(const char* nm, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6, const char* a7, const char* a8, const char* a9, const char* a10, const char* a11) {
    const char* fr[12];
    fr[0] = a0; fr[1] = a1; fr[2] = a2; fr[3] = a3; fr[4] = a4; fr[5] = a5;
    fr[6] = a6; fr[7] = a7; fr[8] = a8; fr[9] = a9; fr[10] = a10; fr[11] = a11;
    return (int)ctron_ext_dispatch0(nm, 12, fr);
}
#else
int ctron_ext_dispatch(const char* nm, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6, const char* a7, const char* a8, const char* a9, const char* a10, const char* a11) { (void)nm; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; ctron_panic("extern 解释口径:Windows β 不支持"); return 0; }
#endif
static const char* ctron_version = "v0.0.1-433-g688f65e";
static const char* ctron_cli_fmt = NULL;
static const char* ctron_cli_prof = NULL;
static const char* ctron_cli_trusted = NULL;
static const char* ctron_cli_dumpgui = NULL;
static const char* ctron_cli_flag(const char* n) { if (!strcmp(n, "format")) { return ctron_cli_fmt ? ctron_cli_fmt : ""; } if (!strcmp(n, "profile")) { return ctron_cli_prof ? ctron_cli_prof : ""; } if (!strcmp(n, "trusted")) { return ctron_cli_trusted ? ctron_cli_trusted : ""; } if (!strcmp(n, "dumpgui")) { return ctron_cli_dumpgui ? ctron_cli_dumpgui : ""; } return ""; }
static char ctron_exe_buf[4096];
static const char* ctron_exe_path(void) {
#if defined(_WIN32)
DWORD ctron_en = GetModuleFileNameA(NULL, ctron_exe_buf, 4096); return (ctron_en > 0 && ctron_en < 4096) ? ctron_exe_buf : "";
#elif defined(__APPLE__)
uint32_t ctron_esz = 4096; return _NSGetExecutablePath(ctron_exe_buf, &ctron_esz) == 0 ? ctron_exe_buf : "";
#elif defined(__linux__)
long ctron_el = readlink("/proc/self/exe", ctron_exe_buf, 4095); if (ctron_el > 0) { ctron_exe_buf[ctron_el] = 0; return ctron_exe_buf; } return "";
#else
return "";
#endif
}
static const char* ctron_env_get(const char* n) { const char* v = getenv(n); return v ? v : ""; }
typedef struct { int32_t* d; int64_t n; } ctron_view_i;
typedef struct { int64_t* d; int64_t n; } ctron_view_6;
typedef struct { int* d; int64_t n; } ctron_view_b;
typedef struct { const char** d; int64_t n; } ctron_view_s;
typedef struct { double* d; int64_t n; } ctron_view_f;
typedef struct { float* d; int64_t n; } ctron_view_g;
typedef struct { const char* code; const char* ctype; const char* extra; const char* body; int64_t reload; int64_t stop; } t_Res;
typedef struct { const char* key; int32_t iters; int64_t port; const char* dir; const char* f_users; const char* f_todos; } t_Cfg;
typedef struct { int64_t ms; int64_t ml; int64_t ts; int64_t tl; int64_t hend; int64_t n; int64_t good; } t_ReqH;
typedef struct { const char* method; const char* target; const char* body; const char* cookie; } t_Basics;
typedef struct { } t_StdNet;
typedef struct { const char* message; } t_NetError;
typedef struct { int64_t fd; int32_t port; } t_TcpListener;
typedef struct { int64_t fd; } t_TcpStream;
typedef struct { int64_t fd; } t_UdpSock;
typedef struct { int64_t fd; const char* path; } t_UnixListener;
typedef struct { int64_t fd; } t_UnixStream;
typedef struct { int64_t v; } t_Box64;
typedef struct { int64_t max_line; int64_t max_headers; int64_t max_header; int64_t max_head; int64_t max_body; } t_HttpLimits;
typedef struct { int64_t rc; int64_t err; int64_t is_resp; int64_t ver; int64_t method_start; int64_t method_len; int64_t target_start; int64_t target_len; int64_t status; int64_t head_end; int64_t nheaders; int64_t body_kind; int64_t clen; } t_HttpHead;
typedef struct { int64_t rc; int64_t ns; int64_t nl; int64_t vs; int64_t vl; } t_HttpSlice;
typedef struct { int64_t rc; int64_t pos; } t_HttpScan;
typedef struct { int64_t h0; int64_t h1; int64_t h2; int64_t h3; int64_t h4; int64_t h5; int64_t h6; int64_t h7; } t_Sha256St;
typedef struct { int64_t i0; int64_t i1; int64_t i2; int64_t i3; int64_t i4; int64_t i5; int64_t i6; int64_t i7; int64_t o0; int64_t o1; int64_t o2; int64_t o3; int64_t o4; int64_t o5; int64_t o6; int64_t o7; } t_HmacCtx;
typedef struct { int32_t y; int32_t m; int32_t d; } t_CivilDate;
static void t_main();
static t_Res t_r_red(const char* t_loc, const char* t_extra);
static t_Res t_r_html(const char* t_body);
static t_Res t_r_html_err(const char* t_body);
static t_Res t_r_json(const char* t_body);
static t_Res t_r_text(const char* t_code, const char* t_body);
static t_Res t_r_stop();
static const char* t_lane_str(ctron_view_6 t_buf, int64_t t_a, int64_t t_b);
static int64_t t_hfind(ctron_view_6 t_buf, int64_t t_n, const char* t_needle);
static int64_t t_fill(ctron_view_6 t_buf, const char* t_s);
static const char* t_hval(ctron_view_6 t_buf, int64_t t_n, const char* t_name);
static int64_t t_cslot(const char* t_code);
static void t_respond_res(ctron_view_6 t_cnt, t_StdNet t_net, int64_t t_fd, ctron_view_6 t_buf, t_Res t_r);
static const char* t_form_fld(ct_res t_ps, const char* t_key, const char* t_dft);
static int t_pfx(const char* t_target, const char* t_pre);
static int64_t t_path_id(const char* t_target, const char* t_pre);
static t_Cfg t_cfg();
static int64_t t_listen_at(t_StdNet t_net, int64_t t_port);
static t_ReqH t_read_req(t_StdNet t_net, int64_t t_fd, ctron_view_6 t_buf, ctron_view_6 t_scr);
static t_Basics t_basics(ctron_view_6 t_buf, t_ReqH t_rh);
static const char* t_sess_uid(const char* t_key, const char* t_cookie_hdr);
static t_Res t_h_health(int64_t t_served, int64_t t_n_users, int64_t t_n_todos);
static t_Res t_h_metrics(ctron_view_6 t_cnt, int64_t t_n_users, int64_t t_n_todos);
static t_Res t_h_login_get();
static t_Res t_h_login_post(ctron_list* t_users, const char* t_key, const char* t_body);
static t_Res t_h_register(ctron_list* t_users, int32_t t_iters, const char* t_body, const char* t_path);
static t_Res t_h_logout();
static t_Res t_h_app(const char* t_uid, ctron_list* t_todos);
static t_Res t_h_add(ctron_list* t_todos, const char* t_uid, const char* t_title, int64_t t_next_id, const char* t_path);
static t_Res t_h_toggle(ctron_list* t_todos, const char* t_uid, int64_t t_id, const char* t_path);
static t_Res t_h_delete(ctron_list* t_todos, const char* t_uid, int64_t t_id, const char* t_path);
static t_Res t_route_app(const char* t_f_todos, ctron_list* t_todos, const char* t_uid, const char* t_method, const char* t_target, const char* t_body);
static t_Res t_route(const char* t_key, int32_t t_iters, const char* t_f_users, const char* t_f_todos, ctron_list* t_users, ctron_list* t_todos, const char* t_uid, t_Basics t_b, int64_t t_served, ctron_view_6 t_cnt);
static void t_run_with(ct_clop t_f);
static void t_app_main();
static void t_serve();
static t_StdNet t_Net_probe();
static t_NetError t_last_net_error();
static int64_t t_addr_port(const char* t_addr);
static const char* t_addr_host(const char* t_addr);
static const char* t_addr_format(const char* t_host, int64_t t_port);
static int64_t t_net_tcp_listen(t_StdNet t_net, const char* t_host, int64_t t_port, t_Box64* t_out_fd);
static int64_t t_net_tcp_sockname(t_StdNet t_net, int64_t t_fd, t_Box64* t_out_port);
static int64_t t_net_tcp_accept(t_StdNet t_net, int64_t t_lfd, t_Box64* t_out_fd);
static int64_t t_net_read_t(t_StdNet t_net, int64_t t_fd, ctron_view_6 t_buf, int64_t t_cap, int64_t t_timeout_ms);
static int64_t t_net_write(t_StdNet t_net, int64_t t_fd, ctron_view_6 t_buf, int64_t t_n);
static int64_t t_net_close(t_StdNet t_net, int64_t t_fd);
extern int64_t ctron_net_now_ns();
extern int64_t ctron_net_sleep_ms(int64_t t_ms);
extern int64_t ctron_net_last_errno();
extern const char* ctron_net_strerror(int64_t t_e);
extern const char* str_from_c(const char* t_s);
extern int64_t ctron_net_tcp_listen(const char* t_host, int64_t t_port, t_Box64* t_out_fd);
extern int64_t ctron_net_tcp_sockname(int64_t t_fd, t_Box64* t_out_port);
extern int64_t ctron_net_tcp_accept(int64_t t_lfd, t_Box64* t_out_fd);
extern int64_t ctron_net_read_t(int64_t t_fd, ctron_view_6 t_buf, int64_t t_cap, int64_t t_timeout_ms);
extern int64_t ctron_net_write(int64_t t_fd, ctron_view_6 t_buf, int64_t t_n);
extern int64_t ctron_net_close(int64_t t_fd);
static t_HttpLimits t_http_limits_default();
static int64_t t_http_err_line();
static int64_t t_http_err_line_long();
static int64_t t_http_err_version();
static int64_t t_http_err_token();
static int64_t t_http_err_target();
static int64_t t_http_err_hdr();
static int64_t t_http_err_fold();
static int64_t t_http_err_hdr_long();
static int64_t t_http_err_hdr_many();
static int64_t t_http_err_head_long();
static int64_t t_http_err_smuggle();
static int64_t t_http_err_cl();
static int64_t t_http_err_te();
static int64_t t_http_err_bodycap();
static t_HttpHead t_hs_need();
static t_HttpHead t_hs_fail(int64_t t_code);
static int64_t t_hs_tchar(int64_t t_c);
static int64_t t_hs_vchar(int64_t t_c);
static int64_t t_hs_lower(int64_t t_c);
static int64_t t_hs_text_ok(ctron_view_6 t_buf, int64_t t_s, int64_t t_e);
static t_HttpScan t_hs_scan_line(ctron_view_6 t_buf, int64_t t_n, int64_t t_start, int64_t t_cap);
static int64_t t_hs_name_eq(ctron_view_6 t_buf, int64_t t_ns, int64_t t_nl, const char* t_name);
static int64_t t_hs_token_list_has(ctron_view_6 t_buf, int64_t t_vs, int64_t t_vl, const char* t_tok);
static int64_t t_hs_parse_dec(ctron_view_6 t_buf, int64_t t_vs, int64_t t_vl);
static t_HttpHead t_http_parse_head(ctron_view_6 t_buf, int64_t t_n, t_HttpLimits t_lim);
static int64_t t_hs_token_list_has_last(ctron_view_6 t_buf, int64_t t_vs, int64_t t_vl, const char* t_tok);
static t_HttpSlice t_http_header_at(ctron_view_6 t_buf, t_HttpHead t_h, int64_t t_idx);
static int64_t t_http_header_find(ctron_view_6 t_buf, t_HttpHead t_h, const char* t_name);
static t_HttpSlice t_http_header_value(ctron_view_6 t_buf, t_HttpHead t_h, const char* t_name);
static int64_t t_hs_head_lines_start(ctron_view_6 t_buf, t_HttpHead t_h);
static int64_t t_http_keep_alive(ctron_view_6 t_buf, t_HttpHead t_h);
static int64_t t_http_rc(t_HttpHead t_h);
static int64_t t_http_err_of(t_HttpHead t_h);
static int64_t t_http_is_resp(t_HttpHead t_h);
static int64_t t_http_ver(t_HttpHead t_h);
static int64_t t_http_status_of(t_HttpHead t_h);
static int64_t t_http_head_end(t_HttpHead t_h);
static int64_t t_http_nheaders(t_HttpHead t_h);
static int64_t t_http_body_kind(t_HttpHead t_h);
static int64_t t_http_clen(t_HttpHead t_h);
static int64_t t_http_method_start(t_HttpHead t_h);
static int64_t t_http_method_len(t_HttpHead t_h);
static int64_t t_http_target_start(t_HttpHead t_h);
static int64_t t_http_target_len(t_HttpHead t_h);
static int64_t t_sl_rc(t_HttpSlice t_s);
static int64_t t_sl_vs(t_HttpSlice t_s);
static int64_t t_sl_vl(t_HttpSlice t_s);
static const char* t_mx_esc(const char* t_v);
static const char* t_mx_meta(const char* t_name, const char* t_typ, const char* t_help);
static const char* t_mx_line(const char* t_name, const char* t_lname, const char* t_lval, int64_t t_val);
static int32_t t_f_hexval(int32_t t_c);
static const char* t_f_byte(int32_t t_b);
static ct_res t_f_pct_decode(const char* t_s);
static ct_res t_form_parse(const char* t_s);
static const char* t_f_pair_key(const char* t_s);
static const char* t_f_pair_val(const char* t_s);
static int32_t t_form_count(ctron_list* t_ps);
static const char* t_form_key_at(ctron_list* t_ps, int32_t t_i);
static const char* t_form_val_at(ctron_list* t_ps, int32_t t_i);
static ct_res t_parse_i64(const char* t_s);
static ct_res t_parse_bool(const char* t_s);
static int64_t t_sc_get_l(ct_res t_o, int64_t t_dft);
static int t_sc_get_b(ct_res t_o, int t_dft);
static int t_sc_some_l(ct_res t_o);
static int t_sc_some_b(ct_res t_o);
static ctron_list* t_words(const char* t_s);
static int t_contains(const char* t_s, const char* t_sub);
static ctron_list* t_lines(const char* t_s);
static int32_t t_count_ch(const char* t_s, int32_t t_ch);
static const char* t_join(ctron_list* t_parts, const char* t_sep);
static int t_starts_with(const char* t_s, const char* t_pre);
static int t_ends_with(const char* t_s, const char* t_suf);
static const char* t_trim(const char* t_s);
static ctron_list* t_split(const char* t_s, int32_t t_sep);
static const char* t_reverse(const char* t_s);
static const char* t_replace(const char* t_s, const char* t_from, const char* t_to);
static const char* t_to_lower(const char* t_s);
static const char* t_to_upper(const char* t_s);
static const char* t_pad_left(const char* t_s, int32_t t_width, const char* t_pad);
static const char* t_pad_right(const char* t_s, int32_t t_width, const char* t_pad);
static const char* t_char_at(const char* t_s, int32_t t_i);
static int32_t t_index_of(const char* t_s, const char* t_sub);
static int32_t t_index_of_from(const char* t_s, const char* t_sub, int32_t t_from);
static ctron_list* t_split_str(const char* t_s, const char* t_sep);
static const char* t_repeat(const char* t_s, int32_t t_n);
static int t_is_ascii_digit(int32_t t_b);
static int t_is_ascii_alpha(int32_t t_b);
static int t_is_ascii_space(int32_t t_b);
static ct_res t_strip_prefix(const char* t_s, const char* t_pre);
static ct_res t_strip_suffix(const char* t_s, const char* t_suf);
static int32_t t_count_sub(const char* t_s, const char* t_sub);
static int t_eq_ignore_ascii_case(const char* t_a, const char* t_b);
static const char* t_str_get(ct_res t_o, const char* t_dft);
static int t_str_some(ct_res t_o);
static const char* t_trim_left(const char* t_s);
static const char* t_trim_right(const char* t_s);
static ctron_list* t_partition(const char* t_s, const char* t_sep);
static int32_t t_rindex(const char* t_s, const char* t_sub);
static const char* t_replace_n(const char* t_s, const char* t_from, const char* t_to, int32_t t_n);
static ctron_list* t_split_n(const char* t_s, const char* t_sep, int32_t t_max);
static int t_is_ascii_upper(int32_t t_b);
static int t_is_ascii_lower(int32_t t_b);
static int t_is_ascii_xdigit(int32_t t_b);
static const char* t_jesc(const char* t_s);
static const char* t_now_iso();
static ctron_list* t_sbytes(const char* t_s);
static const char* t_dstr(int64_t t_v);
static const char* t_fld_s(const char* t_line, const char* t_path);
static int64_t t_fld_i(const char* t_line, const char* t_path, int64_t t_dft);
static int t_fld_b(const char* t_line, const char* t_path, int t_dft);
static const char* t_jstr(const char* t_line, const char* t_want);
static int32_t t_hfind_str(const char* t_s, const char* t_want);
static int32_t t_hxv(int32_t t_c);
static const char* t_one_ascii(int32_t t_v);
static const char* t_b2x(ctron_list* t_b);
static const char* t_pw_hash(const char* t_pass, const char* t_salt_hex, int32_t t_iters);
static const char* t_salt_new(const char* t_uid, int64_t t_seed);
static int t_uid_ok(const char* t_s);
static const char* t_user_line(const char* t_uid, const char* t_salt, const char* t_hash, int32_t t_iters, const char* t_created);
static const char* t_todo_line(int64_t t_id, const char* t_uid, const char* t_title, int t_done, const char* t_created);
static int64_t t_user_validate(ctron_list* t_users, const char* t_uid, const char* t_pass);
static int t_user_auth(ctron_list* t_users, const char* t_uid, const char* t_pass);
static ctron_list* t_todos_of(ctron_list* t_todos, const char* t_uid);
static ctron_list* t_todo_toggle(ctron_list* t_todos, const char* t_uid, int64_t t_id);
static ctron_list* t_todo_delete(ctron_list* t_todos, const char* t_uid, int64_t t_id);
static int64_t t_max_todo_id(ctron_list* t_todos);
static ctron_list* t_load_file(const char* t_path);
static int t_save_file(const char* t_path, ctron_list* t_lines);
static int64_t t_z64();
static int64_t t_p2(int32_t t_k);
static int64_t t_xor32(int64_t t_a, int64_t t_b);
static int64_t t_and32(int64_t t_a, int64_t t_b);
static int64_t t_not32(int64_t t_a);
static int64_t t_rotr32(int64_t t_v, int32_t t_n);
static int64_t t_shr32(int64_t t_v, int32_t t_n);
static int64_t t_msgb(const char* t_msg, int32_t t_g);
static const char* t_hexch(int64_t t_v);
static const char* t_hex_word(int64_t t_h);
static int64_t t_kk_at(int32_t t_i);
static int64_t t_block_word(const char* t_msg, int32_t t_g, int32_t t_n, int32_t t_fin, int32_t t_qlen, int64_t t_bits);
static const char* t_sha256_hex(const char* t_msg);
static int64_t t_s1_or32(int64_t t_a, int64_t t_b);
static int64_t t_s1_rotl(int64_t t_v, int32_t t_n);
static int64_t t_s1_f(int32_t t_t, int64_t t_b, int64_t t_c, int64_t t_d);
static int64_t t_s1_k(int32_t t_t);
static const char* t_sha1_hex(const char* t_msg);
static t_Sha256St t_s256_iv();
static t_Sha256St t_s256_step(t_Sha256St t_st, int64_t t_w0, int64_t t_w1, int64_t t_w2, int64_t t_w3, int64_t t_w4, int64_t t_w5, int64_t t_w6, int64_t t_w7, int64_t t_w8, int64_t t_w9, int64_t t_w10, int64_t t_w11, int64_t t_w12, int64_t t_w13, int64_t t_w14, int64_t t_w15);
static int64_t t_s256_lbyte(ctron_list* t_msg, int32_t t_k);
static int64_t t_s256_w64(ctron_list* t_b, int32_t t_o);
static int64_t t_s256_word(ctron_list* t_msg, int32_t t_g, int32_t t_off, int32_t t_total, int32_t t_qlen, int64_t t_bits);
static ctron_list* t_s256_absorb(t_Sha256St t_st, ctron_list* t_tail, int32_t t_off);
static ctron_list* t_sha256_bytes(ctron_list* t_msg);
static t_HmacCtx t_hmac_ctx(ctron_list* t_key);
static ctron_list* t_hmac_do(t_HmacCtx t_cx, ctron_list* t_msg);
static ctron_list* t_hmac_sha256(ctron_list* t_key, ctron_list* t_msg);
static ctron_list* t_pbkdf2_sha256(ctron_list* t_pass, ctron_list* t_salt, int32_t t_iters, int32_t t_dklen);
static const char* t_bytes_to_hex(ctron_list* t_b);
static int32_t t_s256_hexval(int32_t t_c);
static ctron_list* t_bytes_from_hex(const char* t_s);
static int64_t t_rng_seed(int64_t t_s);
static int64_t t_rng_fresh();
static int64_t t_rng_next(int64_t t_s);
static int32_t t_rng_range(int64_t t_s, int32_t t_lo, int32_t t_hi);
static const char* t_read_or(const char* t_path, const char* t_dft);
static int t_exists(const char* t_path);
static int t_leap_is_leap(int32_t t_y);
static int32_t t_leap_days_in_month(int32_t t_y, int32_t t_m);
static int32_t t_days_from_civil(int32_t t_y, int32_t t_m, int32_t t_d);
static t_CivilDate t_civil_from_days(int32_t t_z);
static int32_t t_weekday(int32_t t_z);
static const char* t_date_format(int32_t t_z);
static int32_t t_t_digits2(const char* t_s, int32_t t_i);
static int32_t t_t_digits4(const char* t_s);
static ct_res t_date_parse(const char* t_s);
static int32_t t_time_get(ct_res t_o, int32_t t_dft);
static int t_time_some(ct_res t_o);
static int64_t t_time_to_epoch_s(int32_t t_y, int32_t t_m, int32_t t_d);
static const char* t_two(int64_t t_v);
static const char* t_iso_utc(int64_t t_ms);
static const char* t_now_iso_utc();
static const char* t_sdstr(int64_t t_v);
static ctron_list* t_ssbytes(const char* t_s);
static const char* t_sb2x(ctron_list* t_b);
static int t_suid_ok(const char* t_s);
static int t_c