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
typedef struct ct_chan { pthread_mutex_t mu; pthread_cond_t cv_full, cv_empty; ct_i* buf; int head, tail, cnt, cap; } ct_chan;
static ct_chan** ct_chans = 0; static int ct_nchans = 0, ct_chancap = 0; static pthread_mutex_t ct_chreg = PTHREAD_MUTEX_INITIALIZER;
static void ct_ch_reg(ct_chan* c) { pthread_mutex_lock(&ct_chreg); if (ct_nchans == ct_chancap) { int nc = ct_chancap ? ct_chancap * 2 : 16; ct_chan** na = (ct_chan**)realloc(ct_chans, (size_t)nc * sizeof(ct_chan*)); if (na) { ct_chans = na; ct_chancap = nc; } } if (ct_nchans < ct_chancap) { ct_chans[ct_nchans] = c; ct_nchans += 1; } pthread_mutex_unlock(&ct_chreg); }
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
static ct_chan* ct_ch_make(int cap) { if (cap < 1) { cap = 1; } ct_chan* c = (ct_chan*)calloc(1, sizeof(ct_chan)); pthread_mutex_init(&c->mu, 0); pthread_cond_init(&c->cv_full, 0); pthread_cond_init(&c->cv_empty, 0); c->cap = cap; c->buf = (ct_i*)calloc((size_t)cap, sizeof(ct_i)); ct_ch_reg(c); return c; }
static ct_res ct_ch_send(ct_chan* c, ct_i v) { ct_res res; pthread_mutex_lock(&c->mu); while (c->cnt == c->cap) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_full, &c->mu); } c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->cnt += 1; pthread_cond_broadcast(&c->cv_empty); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); res.variant = 0; res.v = 0; return res; }
static ct_res ct_ch_recv(ct_chan* c) { ct_res res; pthread_mutex_lock(&c->mu); while (c->cnt == 0) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_empty, &c->mu); } res.variant = 0; res.v = c->buf[c->head]; c->head = (c->head + 1) % c->cap; c->cnt -= 1; pthread_cond_broadcast(&c->cv_full); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); return res; }
static inline int32_t ct_bit_shl_i32(int32_t a, int32_t n) { if (n < 0 || n >= 32) { ctron_panic("bit shift range"); } return (int32_t)(((uint32_t)a) << n); }
static inline uint32_t ct_bit_shl_u32(uint32_t a, int32_t n) { if (n < 0 || n >= 32) { ctron_panic("bit shift range"); } return (uint32_t)(((uint32_t)a) << n); }
static inline int64_t ct_bit_shl_i64(int64_t a, int32_t n) { if (n < 0 || n >= 64) { ctron_panic("bit shift range"); } return (int64_t)(((uint64_t)a) << n); }
static inline uint64_t ct_bit_shl_u64(uint64_t a, int32_t n) { if (n < 0 || n >= 64) { ctron_panic("bit shift range"); } return (uint64_t)(((uint64_t)a) << n); }
static inline int32_t ct_bit_shr_i32(int32_t a, int32_t n) { if (n < 0 || n >= 32) { ctron_panic("bit shift range"); } return a >> n; }
static inline uint32_t ct_bit_shr_u32(uint32_t a, int32_t n) { if (n < 0 || n >= 32) { ctron_panic("bit shift range"); } return (uint32_t)(((uint32_t)a) >> n); }
static inline int64_t ct_bit_shr_i64(int64_t a, int32_t n) { if (n < 0 || n >= 64) { ctron_panic("bit shift range"); } return a >> n; }
static inline uint64_t ct_bit_shr_u64(uint64_t a, int32_t n) { if (n < 0 || n >= 64) { ctron_panic("bit shift range"); } return (uint64_t)(((uint64_t)a) >> n); }
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
static const char* ctron_version = "v0.0.1-597-g83a4d6a";
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
static int32_t t_main();
int32_t t_main() 
{
int64_t t_c = 9223372036854775807LL;
t_c = ctron_i64_add(t_c, 1);
printf("%s\n", (const char*)(ctron_i64_to_string(t_c)));
return 0;
}
int main(int argc, char** argv) { if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; } { int ctron_ai; for (ctron_ai = 3; ctron_ai < argc; ctron_ai++) { if (!strncmp(argv[ctron_ai], "--format=", 9)) { ctron_cli_fmt = argv[ctron_ai] + 9; } else if (!strncmp(argv[ctron_ai], "--profile=", 10)) { ctron_cli_prof = argv[ctron_ai] + 10; } else if (!strcmp(argv[ctron_ai], "--trusted")) { ctron_cli_trusted = "1"; } else if (!strcmp(argv[ctron_ai], "--dump-gui")) { ctron_cli_dumpgui = "1"; } } } 
#if defined(_WIN32) 
SetConsoleOutputCP(CP_UTF8); 
_setmode(_fileno(stdout), _O_BINARY); 
#endif
 if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; } return (int)t_main(); }
