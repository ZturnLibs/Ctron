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
static const char* ctron_anchor = "ANCHORINPUT";
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
static const char* ctron_version = "v0.0.1-510-gff7df81";
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
typedef struct { int32_t tag; int64_t iv; int b; const char* s; ctron_list* cx; } t_Val;
typedef struct { const char* kind; ctron_list* env; t_Val v; const char* out; } t_Flow;
static ctron_list* t_VNIL = 0;
static void ctron_statics_init(void) { t_VNIL = ctron_list_new(); }
static int ctron_exp_init = 0;
static int t_or2(int t_a, int t_b);
static int t_or3(int t_a, int t_b, int t_c);
static int t_is_digit(int32_t t_b);
static int t_is_alpha(int32_t t_b);
static int t_is_al(int32_t t_b);
static int t_is_ac(int32_t t_b);
static int t_is_hex(int32_t t_b);
static int t_is_oct(int32_t t_b);
static int t_is_bin(int32_t t_b);
static int32_t t_suffix_len(const char* t_src, int32_t t_n, int32_t t_i);
static int t_nl_set_line_end(const char* t_t);
static int t_nl_set_next(const char* t_t);
static void t_filter_nl(ctron_list* t_raw, ctron_list* t_rlines, ctron_list* t_rcols, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static void t_pdiag(ctron_list* t_pdiags, ctron_list* t_lns, ctron_list* t_cols, ctron_cell* t_cur, const char* t_code, const char* t_msg);
static int32_t t_scan5(const char* t_src, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags, ctron_list* t_tstarts, ctron_list* t_tends, ctron_list* t_cstarts, ctron_list* t_cends);
static void t_scan4(const char* t_src, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static int t_lex_single(int32_t t_b);
static void t_scan(const char* t_src, ctron_list* t_toks, ctron_list* t_lns);
static void t_rpush(ctron_list* t_raw, ctron_list* t_rlines, ctron_list* t_rcols, ctron_list* t_tstarts, ctron_list* t_tends, int32_t t_line, int32_t t_col, int32_t t_s, int32_t t_e, const char* t_t);
static const char* t_tok(ctron_list* t_toks, ctron_cell* t_cur);
static void t_adv(ctron_cell* t_cur);
static const char* t_num_text(const char* t_t);
static const char* t_num_sfx(const char* t_t);
static const char* t_diag_lang();
static const char* t_diag_tpl(const char* t_key);
static const char* t_diag_tpl_en(const char* t_key);
static const char* t_diag_tpl_zh(const char* t_key);
static const char* t_diag_subst(const char* t_tpl, ctron_list* t_args);
static const char* t_diag_code(const char* t_key);
static const char* t_diag_text(const char* t_key, ctron_list* t_args);
static const char* t_diag_text0(const char* t_key);
static const char* t_diag_text1(const char* t_key, const char* t_a0);
static const char* t_diag_render(const char* t_key, const char* t_line, ctron_list* t_args);
static const char* t_diag_render0(const char* t_key, const char* t_line);
static const char* t_diag_render1(const char* t_key, const char* t_line, const char* t_a0);
static void t_diag_emit(ctron_list* t_diags, const char* t_key, const char* t_line, ctron_list* t_args);
static void t_diag0(ctron_list* t_diags, const char* t_key, const char* t_line);
static void t_diag1(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0);
static void t_diag2(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1);
static void t_diag3(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2);
static void t_diag4(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2, const char* t_a3);
static void t_diag5(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2, const char* t_a3, const char* t_a4);
static void t_diag_once(ctron_list* t_diags, const char* t_key, const char* t_line, ctron_list* t_args);
static void t_donce0(ctron_list* t_diags, const char* t_key, const char* t_line);
static void t_donce1(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0);
static void t_donce2(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1);
static void t_donce3(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2);
static ctron_list* t_mk(const char* t_t);
static void t_nstamp(ctron_list* t_n, const char* t_line);
static void t_ast_put(ctron_list* t_k, ctron_list* t_v, const char* t_key, const char* t_val);
static const char* t_ast_shape(ctron_list* t_k, ctron_list* t_v, const char* t_tag);
static const char* t_ast_kind(const char* t_sh, int32_t t_si);
static void t_ast_tables(ctron_list* t_ktags, ctron_list* t_kshapes);
static const char* t_qtext(const char* t_raw);
static ctron_list* t_parts_of(const char* t_raw);
static ctron_list* t_p_oror(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_and(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_cmp(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_rng(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_add(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_mul(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_una(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_post(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_pri(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_tpars(ctron_list* t_toks, ctron_cell* t_cur);
static ctron_list* t_p_typ(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_or(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_stmt_expr(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al);
static ctron_list* t_p_pattern_or(ctron_list* t_toks, ctron_cell* t_cur);
static ctron_list* t_p_pattern(ctron_list* t_toks, ctron_cell* t_cur);
static ctron_list* t_p_match(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_if(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_block(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_fn2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_tag);
static ctron_list* t_p_fn(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_test(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_field(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_struct2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_dr, const char* t_anm);
static ctron_list* t_p_struct(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_enum2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_dr, const char* t_anm);
static ctron_list* t_p_prop_item2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_kind);
static ctron_list* t_p_items(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_kind);
static ctron_list* t_p_class(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_impl(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static int t_type_start(const char* t_t);
static ctron_list* t_p_trait(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_enum(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static int t_p_kw(const char* t_t);
static const char* t_p_use_alias(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static ctron_list* t_p_use(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static ctron_list* t_p_const(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_static(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns);
static ctron_list* t_p_extern(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags, const char* t_anm);
static ctron_list* t_p_file(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags);
static ctron_list* t_gui_block(ctron_list* t_toks, ctron_cell* t_cur, const char* t_kind);
static int32_t t_gui_split(const char* t_joined, ctron_list* t_out);
static int32_t t_gui_hex1(int32_t t_b);
static const char* t_gui_unq(const char* t_v);
static const char* t_eat_tok(ctron_list* t_tks, ctron_cell* t_cur);
static int32_t t_gui_parse_color(const char* t_v);
static int32_t t_gui_dump_file(ctron_list* t_file);
static int32_t t_gui_lower_element(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out);
static int32_t t_gui_lower_view(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out);
static int32_t t_gui_lower_style(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out);
static const char* t_gui_blocks_src(ctron_list* t_file);
static int32_t t_gui_field_tables(ctron_list* t_file, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv);
static int32_t t_gui_ds_views(ctron_list* t_file, ctron_list* t_vnames, ctron_list* t_vprops);
static const char* t_gui_ds_find_expr(ctron_list* t_e, ctron_list* t_views, ctron_list* t_names);
static const char* t_gui_ds_find_block(ctron_list* t_b, ctron_list* t_views, ctron_list* t_names);
static const char* t_gui_ds_find_file(ctron_list* t_file, ctron_list* t_views, ctron_list* t_names);
static const char* t_gui_ds_path_ty(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv);
static const char* t_gui_ds_rw(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptoks);
static int t_gui_is_ctml(const char* t_p);
static int32_t t_gui_ctml_tokens(const char* t_src, ctron_list* t_out);
static ctron_list* t_gui_ctml_file(const char* t_src);
static int t_gui_ck_has_style(ctron_list* t_sn, const char* t_name);
static void t_gui_bx_roots(const char* t_e, ctron_list* t_out);
static int32_t t_gui_fn_arity(ctron_list* t_d);
static int t_gui_ty_is_box(ctron_list* t_t);
static ctron_list* t_gui_fn_ptyps(ctron_list* t_file, const char* t_nm);
static int32_t t_gui_ck_fn_argc(ctron_list* t_fns, ctron_list* t_fna, const char* t_name);
static const char* t_gui_ev_head2(const char* t_e, ctron_list* t_argc, ctron_list* t_aout);
static void t_gui_bx_dotted(const char* t_e, ctron_list* t_out);
static int32_t t_gui_props_typed(const char* t_d5, ctron_list* t_names, ctron_list* t_types);
static const char* t_gui_trim(const char* t_s);
static const char* t_gui_ty_head_txt(const char* t_t);
static int32_t t_gui_ck_prop_path(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_gdiags, const char* t_where);
static const char* t_gui_ty_head_node(ctron_list* t_t);
static int32_t t_gui_ck_bx_gate(const char* t_e, ctron_list* t_props, ctron_list* t_ptyps, ctron_list* t_pscope, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_gdiags, const char* t_where);
static int32_t t_gui_ck_color(const char* t_v, ctron_list* t_gdiags);
static int32_t t_gui_ck_tag(const char* t_tag, ctron_list* t_gdiags);
static int32_t t_gui_ck_elem(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_sn, ctron_list* t_fns, ctron_list* t_fna, ctron_list* t_gdiags, ctron_list* t_gwarns, int32_t t_depth, ctron_cell* t_sawin, int t_indep, ctron_list* t_props, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_pscope, ctron_list* t_vr_nm, ctron_list* t_vr_pp, int t_iscomp, ctron_list* t_fbox);
static int32_t t_gui_props_names(const char* t_d5, ctron_list* t_out);
static int32_t t_gui_check_file(ctron_list* t_file, ctron_list* t_gdiags, ctron_list* t_gwarns, int t_indep);
static const char* t_gui_sk_word(ctron_list* t_tks, ctron_cell* t_cur);
static const char* t_gui_bx_expr(ctron_list* t_tks, ctron_cell* t_cur);
static int t_gui_bx_impure(const char* t_e);
static int32_t t_gui_sk_attrs(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_out4);
static int32_t t_gui_sk_node(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_ntag, ctron_list* t_nflag, ctron_list* t_ncls, ctron_list* t_npre, ctron_list* t_nbid, ctron_list* t_npost, ctron_list* t_nfc, ctron_list* t_ns, ctron_list* t_nes, ctron_list* t_nec, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_btns, int32_t t_depth);
static int32_t t_gui_sk_style(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_ext);
static int32_t t_gui_sk_build(ctron_list* t_file, ctron_list* t_ntag, ctron_list* t_nflag, ctron_list* t_ncls, ctron_list* t_npre, ctron_list* t_nbid, ctron_list* t_npost, ctron_list* t_nfc, ctron_list* t_ns, ctron_list* t_nes, ctron_list* t_nec, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_btns, ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_props);
static int32_t t_gui_sk_ext_resolve(ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_ext);
static int t_gui_sk_has(ctron_list* t_sk, const char* t_key);
static int32_t t_gui_sk_dump_all(ctron_list* t_file);
static const char* t_gui_sk_cesc(const char* t_s);
static const char* t_gui_sk_emit_str_arr(int32_t t_id, ctron_list* t_tbl);
static const char* t_gui_sk_emit_int_arr(int32_t t_id, ctron_list* t_tbl);
static ctron_list* t_gui_sk_wrap_s(ctron_list* t_src);
static ctron_list* t_gui_sk_wrap_i(ctron_list* t_src);
static ctron_list* t_gui_ds_ptyps(ctron_list* t_file, ctron_list* t_pnames);
static int32_t t_gui_ds_collect_slots(ctron_list* t_file, ctron_list* t_pnames, ctron_list* t_paths, ctron_list* t_pty, ctron_list* t_lists, ctron_list* t_evn, ctron_list* t_evf, ctron_list* t_inps, ctron_list* t_ievs, ctron_list* t_crows, ctron_list* t_cty, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_cvn, ctron_list* t_cvf, ctron_list* t_ciev);
static int t_tag_is_input_evt(const char* t_tag, const char* t_nbid);
static ctron_list* t_gui_ds_gen(ctron_list* t_file, const char* t_vname, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_ordn, ctron_list* t_paths, ctron_list* t_pty, ctron_list* t_lists, ctron_list* t_evn, ctron_list* t_evf, ctron_list* t_inps, ctron_list* t_ievs, ctron_list* t_crows, ctron_list* t_cty, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_cvn, ctron_list* t_cvf, ctron_list* t_ciev);
static const char* t_gui_ds_marker_expr(ctron_list* t_e);
static const char* t_gui_ds_marker_block(ctron_list* t_b);
static const char* t_gui_ds_marker_file(ctron_list* t_file);
static int32_t t_gui_ds_ordn_expr(ctron_list* t_e, const char* t_vname, ctron_list* t_names);
static int32_t t_gui_ds_ordn_block(ctron_list* t_b, const char* t_vname, ctron_list* t_names);
static ctron_list* t_gui_ds_ordn_file(ctron_list* t_file, const char* t_vname);
static int32_t t_gui_ds_rew_expr(ctron_list* t_e, const char* t_vname);
static int32_t t_gui_ds_rew_block(ctron_list* t_b, const char* t_vname);
static int32_t t_gui_ds_rew_file(ctron_list* t_file, const char* t_vname);
static ctron_list* t_gui_ds_premerge(ctron_list* t_file, ctron_list* t_unit);
static const char* t_gui_ds_target(ctron_list* t_file);
static ctron_list* t_gui_ds_sanitize(ctron_list* t_file);
static ctron_list* t_gui_ds_postmerge(ctron_list* t_file, ctron_list* t_unit, ctron_list* t_diags);
static const char* t_pkg_dir_of(const char* t_path);
static int t_segs_std(ctron_list* t_d);
static const char* t_cap_net_fine(const char* t_sym);
static int t_pkg_is_prelude_sym(const char* t_nm);
static void t_pkg_check_caps(ctron_list* t_mf, const char* t_dir, ctron_list* t_diags);
static int t_pkg_ctcl_at(const char* t_t, int32_t t_i, const char* t_key);
static int t_pkg_ctcl_pre(const char* t_t, int32_t t_i);
static int t_pkg_caps_allowed(const char* t_dir, const char* t_cap);
static const char* t_pkg_exe_dir(const char* t_exe);
static int t_pkg_std_installed(const char* t_dir);
static const char* t_pkg_std_root(const char* t_dir);
static ctron_list* t_pkg_load_use(ctron_list* t_file, const char* t_dir, ctron_list* t_stack, ctron_list* t_diags);
static int t_pkg_is_named_kind(const char* t_t);
static int t_pkg_refs_walk(ctron_list* t_d, ctron_list* t_ktags, ctron_list* t_kshapes, ctron_list* t_acc, int32_t t_depth);
static ctron_list* t_pkg_load_use_done(ctron_list* t_file, const char* t_dir, ctron_list* t_stack, ctron_list* t_done, ctron_list* t_diags, int t_top);
static const char* t_ast_read_line(const char* t_ser, ctron_cell* t_cur);
static ctron_list* t_ast_load_node(const char* t_ser, ctron_cell* t_cur);
static int t_is_opt_ret(ctron_list* t_ty);
static int t_is_prelude_name(const char* t_n);
static int t_is_cmp_op(const char* t_op);
static int t_ext_cb_target(ctron_list* t_file, const char* t_nm);
static void t_walk_e(ctron_list* t_file, ctron_list* t_e, int t_ns, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out);
static void t_w8020_check(ctron_list* t_file, ctron_list* t_ce, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out);
static void t_walk_b(ctron_list* t_file, ctron_list* t_b, int t_ns, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out, int t_body);
static int t_s_prim(const char* t_n);
static int t_s_special(const char* t_n);
static ctron_list* t_decl_node(ctron_list* t_file, const char* t_tag, const char* t_name);
static int t_send_of(ctron_list* t_file, ctron_list* t_ty, int32_t t_depth);
static const char* t_env_lookup(ctron_list* t_envN, ctron_list* t_envC, const char* t_nm);
static const char* t_ch_of_type(ctron_list* t_file, ctron_list* t_ty);
static void t_walk6e(ctron_list* t_file, ctron_list* t_e, int t_ow, ctron_list* t_envN, ctron_list* t_envC, ctron_list* t_diags);
static void t_walk6b(ctron_list* t_file, ctron_list* t_b, int t_ow, ctron_list* t_envN, ctron_list* t_envC, ctron_list* t_diags);
static const char* t_sem_e3060(ctron_list* t_file, ctron_list* t_d, const char* t_tag);
static int t_is_trait(ctron_list* t_file, const char* t_nm);
static ctron_list* t_env_ty(ctron_list* t_envN, ctron_list* t_envT, const char* t_nm);
static int t_cap_ty(ctron_list* t_file, ctron_list* t_ty);
static void t_pwalk_e(ctron_list* t_file, ctron_list* t_e, const char* t_code, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static void t_pwalk_b(ctron_list* t_file, ctron_list* t_b, const char* t_code, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static const char* t_sem_cap(ctron_list* t_file, ctron_list* t_d, const char* t_kind);
static void t_cle(ctron_list* t_file, ctron_list* t_e, ctron_list* t_names);
static void t_clb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_names);
static int t_send_env(ctron_list* t_file, const char* t_nm, ctron_list* t_envN, ctron_list* t_envT);
static ctron_list* t_let_ty(ctron_list* t_file, ctron_list* t_st);
static void t_wse(ctron_list* t_file, ctron_list* t_e, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static void t_wsb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static const char* t_sem_spawn2(ctron_list* t_file, ctron_list* t_d, const char* t_tag);
static int t_in_list(ctron_list* t_ls, const char* t_nm);
static ctron_list* t_rm_name(ctron_list* t_ls, const char* t_nm);
static int t_call_root_arena(ctron_list* t_e);
static void t_me(ctron_list* t_file, ctron_list* t_e, ctron_list* t_moved, ctron_list* t_arenaB, ctron_list* t_diags);
static void t_mb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_moved, ctron_list* t_arenaB, ctron_list* t_diags);
static const char* t_sem_mv(ctron_list* t_file, ctron_list* t_d, const char* t_tag);
static int t_seq(const char* t_a, const char* t_b);
static ctron_list* t_variants_of(ctron_list* t_file, ctron_list* t_ty);
static ctron_list* t_ex_resolve(ctron_list* t_file, ctron_list* t_scrut, ctron_list* t_envN, ctron_list* t_envT);
static void t_xe(ctron_list* t_file, ctron_list* t_e, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static void t_xb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags);
static const char* t_sem_exh(ctron_list* t_file, ctron_list* t_d, const char* t_tag);
static int t_has_noalloc(ctron_list* t_node);
static const char* t_mem_target(ctron_list* t_m);
static int t_fn_alloc_sum(ctron_list* t_file, const char* t_nm, ctron_list* t_vis);
static int t_scan_alloc(ctron_list* t_file, ctron_list* t_b, ctron_list* t_vis);
static int t_sc_e(ctron_list* t_file, ctron_list* t_e, ctron_list* t_vis);
static int t_call_allocish(ctron_list* t_file, ctron_list* t_e, ctron_list* t_vis);
static const char* t_member_root(ctron_list* t_m);
static void t_al_e(ctron_list* t_file, ctron_list* t_e, int t_ow, int t_na, ctron_list* t_arenaB, ctron_list* t_gcL, ctron_list* t_diags);
static void t_al_b(ctron_list* t_file, ctron_list* t_b, int t_ow, int t_na, ctron_list* t_arenaB, ctron_list* t_gcL, ctron_list* t_diags);
static const char* t_sem_alloc(ctron_list* t_file, ctron_list* t_d, const char* t_tag, const char* t_prof);
static int t_trait_method_noalloc(ctron_list* t_file, const char* t_tr, const char* t_mnm);
static const char* t_sem_walk2(ctron_list* t_file, const char* t_prof);
static int t_attr_is(const char* t_v, const char* t_name);
static int t_attr_has(const char* t_v, const char* t_name);
static const char* t_attr_arg(const char* t_v);
static int t_ext_nonabi_ty(ctron_list* t_ty);
static int t_seq2(const char* t_a, const char* t_b);
static int t_prelude_ok(const char* t_nm);
static int t_value_ok(const char* t_nm);
static int32_t t_prelude_arity(const char* t_nm);
static const char* t_root_name(ctron_list* t_e);
static void t_bind_pat(ctron_list* t_pp, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_tn);
static const char* t_ty_kind(ctron_list* t_ty);
static int t_k_int5(const char* t_k);
static int t_k_uint5(const char* t_k);
static int t_k_int(const char* t_k);
static int t_k_num(const char* t_k);
static int t_k_strish(const char* t_k);
static int t_all_int_lit(ctron_list* t_e);
static void t_lit_fit_gate(const char* t_expk, ctron_list* t_e, const char* t_ln, ctron_list* t_diags);
static int t_compat(const char* t_exp, const char* t_act, ctron_list* t_e, ctron_list* t_file, ctron_list* t_tc);
static const char* t_nline(ctron_list* t_n);
static ctron_list* t_unk();
static ctron_list* t_kind_node(const char* t_k);
static int32_t t_idx_of(ctron_list* t_ls, const char* t_nm);
static ctron_list* t_fn_ret_of(ctron_list* t_tc, const char* t_nm);
static const char* t_ex_ty(ctron_list* t_e, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT);
static const char* t_gui_view_props(ctron_list* t_file, const char* t_nm, ctron_list* t_found);
static void t_tce(ctron_list* t_file, ctron_list* t_e, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_retK, ctron_list* t_diags);
static void t_tcb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_retK, ctron_list* t_diags);
static void t_fn_params_push(ctron_list* t_ps, ctron_list* t_loc, ctron_list* t_envT);
static ctron_list* t_find_sdecl(ctron_list* t_file, const char* t_nm);
static int t_has_drop_impl(ctron_list* t_file, const char* t_tnm);
static ctron_list* t_find_gdecl(ctron_list* t_file, const char* t_nm);
static ctron_list* t_subst_tpar_ty(ctron_list* t_ty, ctron_list* t_tps, ctron_list* t_args);
static int t_bound_sat(ctron_list* t_file, ctron_list* t_ty, const char* t_tr, int32_t t_dp);
static void t_check_tpar_bounds(ctron_list* t_file, ctron_list* t_e, ctron_list* t_cal, const char* t_rt, ctron_list* t_diags);
static int t_interp_hit(const char* t_txt, const char* t_nm);
static void t_sem_calls_all(ctron_list* t_file, ctron_list* t_diags, const char* t_prof);
static void t_chk_break(ctron_list* t_file, ctron_list* t_n, int32_t t_depth, int32_t t_loopbase, int32_t t_outer, ctron_list* t_drops, ctron_list* t_diags);
static int t_impl_has_drop(ctron_list* t_file, const char* t_tyname);
static int t_comp_banned_call(const char* t_nm);
static int t_comp_banned_mem(const char* t_nm);
static void t_scan_comp(ctron_list* t_file, ctron_list* t_e, ctron_list* t_diags, const char* t_fnm);
static void t_scb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_diags, const char* t_fnm);
static int t_vtag_ok(const char* t_k, const char* t_vt);
static void t_sem_comp(ctron_list* t_file, ctron_list* t_diags);
static int t_local_type_or_trait(ctron_list* t_file, const char* t_nm);
static int t_ce_budget(ctron_list* t_st);
static ctron_list* t_ceval_expr(ctron_list* t_file, ctron_list* t_e, ctron_list* t_cenvN, ctron_list* t_cenvV, ctron_list* t_st);
static ctron_list* t_ceval_block(ctron_list* t_file, ctron_list* t_b, ctron_list* t_cenvN, ctron_list* t_cenvV, ctron_list* t_st);
static ctron_list* t_cok(t_Val t_v);
static ctron_list* t_cun();
static ctron_list* t_cover();
static void t_mblock(ctron_list* t_file, ctron_list* t_b, ctron_list* t_out);
static void t_mexpr(ctron_list* t_file, ctron_list* t_e, ctron_list* t_out);
static void t_mblock2(ctron_list* t_file, ctron_list* t_out);
static void t_cassign_names(ctron_list* t_e, ctron_list* t_out);
static void t_cblock2(ctron_list* t_b, ctron_list* t_out);
static void t_cscan(ctron_list* t_node, ctron_list* t_muts, ctron_list* t_diags);
static void t_cscan_b(ctron_list* t_b, ctron_list* t_muts, ctron_list* t_diags);
static void t_sem_closures(ctron_list* t_file, ctron_list* t_diags);
static void t_clet_b(ctron_list* t_b, ctron_list* t_out);
static void t_clet_names(ctron_list* t_e, ctron_list* t_out);
static int32_t t_digit_of(int32_t t_c);
static int32_t t_txt_num(const char* t_t);
static const char* t_dvi_str(int32_t t_v);
static int32_t t_wrap_i32(const char* t_u);
static ctron_list* t_cx_wrap(ctron_list* t_l);
static ctron_list* t_cx_of(t_Val t_v);
static t_Val t_vI(int32_t t_x);
static t_Val t_vB(int t_b);
static t_Val t_vS(const char* t_s);
static t_Val t_vV2();
static t_Val t_v6(const char* t_t);
static t_Val t_v7(const char* t_t);
static t_Val t_v6i(int64_t t_x);
static const char* t_v6t(t_Val t_v);
static const char* t_v7t(t_Val t_v);
static t_Val t_vL();
static ctron_list* t_v_box(t_Val t_v);
static t_Val t_v_unbox(ctron_list* t_l);
static t_Val t_cx_val(ctron_list* t_l);
static const char* t_vtag_str(t_Val t_v);
static ctron_list* t_vdeep(ctron_list* t_v);
static int32_t t_dvi(const char* t_t);
static int t_truth(t_Val t_v);
static int t_veq(t_Val t_a, t_Val t_b);
static int32_t t_scmp(const char* t_a, const char* t_b);
static int t_c6_i64ok(const char* t_t);
static int64_t t_c6_i64v(const char* t_t);
static const char* t_c6_i64s(int64_t t_v);
static const char* t_c6can(const char* t_t);
static int32_t t_c6abscmp(const char* t_a, const char* t_b);
static const char* t_lit_digits(const char* t_t);
static int t_c6_in_u64(const char* t_r);
static int t_c6_in_i64(const char* t_r);
static int t_lit_is_dec(const char* t_t);
static const char* t_lit_radix_dec(const char* t_t);
static const char* t_lit_to_dec(const char* t_t);
static int t_lit_wide_i32(const char* t_t);
static int32_t t_c6cmp(const char* t_a, const char* t_b);
static const char* t_c6add(const char* t_a, const char* t_b);
static const char* t_c6addmag(const char* t_a, const char* t_b);
static const char* t_c6submag(const char* t_a, const char* t_b);
static const char* t_c6sub(const char* t_a, const char* t_b);
static const char* t_c6mul(const char* t_a, const char* t_b);
static const char* t_c6divmod(const char* t_a, const char* t_b);
static int32_t t_vcmp(t_Val t_a, t_Val t_b);
static int32_t t_ari(const char* t_op, int32_t t_x, int32_t t_y);
static t_Val t_val_arith(const char* t_op, t_Val t_a, t_Val t_b);
static t_Val t_vW(const char* t_w, int32_t t_x);
static const char* t_w_of(t_Val t_v);
static int32_t t_w_rank(const char* t_w);
static int t_w_signed(const char* t_w);
static int32_t t_w_max(const char* t_w);
static int32_t t_w_min(const char* t_w);
static t_Val t_w_arith(const char* t_op, t_Val t_a, t_Val t_b);
static int32_t t_w_bits(const char* t_w);
static int t_iadd_ov(int32_t t_x, int32_t t_y);
static int t_isub_ov(int32_t t_x, int32_t t_y);
static int t_imul_ov(int32_t t_x, int32_t t_y, int32_t t_mx, int32_t t_mn);
static t_Val t_vD(const char* t_t);
static int64_t t_df_p10(int32_t t_k);
static int32_t t_df_digits(int64_t t_a);
static const char* t_df_pack(int64_t t_m, int32_t t_s);
static int64_t t_df_pm(const char* t_p);
static int32_t t_df_ps(const char* t_p);
static const char* t_df_split(const char* t_t);
static int64_t t_df_al(int64_t t_m, int32_t t_s, int32_t t_t);
static const char* t_df_text(int64_t t_m, int32_t t_s);
static const char* t_df_can(const char* t_t);
static const char* t_df_bin(const char* t_op, const char* t_ta, const char* t_tb);
static int32_t t_df_vcmp(const char* t_ta, const char* t_tb);
static const char* t_df_add1(const char* t_ds);
static const char* t_df_g(const char* t_t);
static const char* t_df_show(const char* t_t);
static const char* t_fmt(t_Val t_v);
static const char* t_fmt_struct(ctron_list* t_v);
static int t_eq_val(t_Val t_a, t_Val t_b);
static ctron_list* t_env_at(ctron_list* t_env, const char* t_nm);
static ctron_list* t_env_add(ctron_list* t_env, const char* t_nm, t_Val t_val);
static ctron_list* t_env_set(ctron_list* t_env, const char* t_nm, t_Val t_val);
static t_Val t_env_at_val(ctron_list* t_env, const char* t_nm);
static ctron_list* t_env_drop(ctron_list* t_env, int32_t t_n);
static ctron_list* t_env_dedupe(ctron_list* t_env, int32_t t_n);
static t_Flow t_e4(const char* t_fl, ctron_list* t_env, t_Val t_vv, const char* t_out);
static t_Flow t_s4(const char* t_fl, ctron_list* t_env, const char* t_out, t_Val t_vv);
static ctron_list* t_expr_of_text(const char* t_s);
static ctron_list* t_find_decl(ctron_list* t_file, const char* t_nm);
static const char* t_unesc(const char* t_s);
static t_Flow t_eval_str(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e);
static ctron_list* t_pm2(const char* t_ok, ctron_list* t_env);
static int t_enum_var(ctron_list* t_file, const char* t_nm);
static const char* t_ty_head(ctron_list* t_ty);
static ctron_list* t_find_trait(ctron_list* t_file, const char* t_nm);
static int t_is_class(ctron_list* t_file, const char* t_nm);
static ctron_list* t_find_impl_method(ctron_list* t_file, const char* t_cls, const char* t_m);
static ctron_list* t_find_impl_prop(ctron_list* t_file, const char* t_cls, const char* t_m);
static t_Flow t_call_method_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_mth, ctron_list* t_sv, ctron_list* t_vals);
static t_Flow t_call_prop_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_pp, ctron_list* t_sv);
static t_Val t_bind_of(ctron_list* t_file, t_Val t_v);
static int t_u_set_ip(ctron_list* t_u, const char* t_nm, ctron_list* t_nv);
static ctron_list* t_u_field(ctron_list* t_u, const char* t_nm);
static ctron_list* t_pat_match(ctron_list* t_pat, t_Val t_v, ctron_list* t_env);
static t_Flow t_eval_expr(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e);
static const char* t_recv_cname(t_Val t_v);
extern int32_t ctron_ext_dispatch(const char* t_nm, const char* t_a0, const char* t_a1, const char* t_a2, const char* t_a3, const char* t_a4, const char* t_a5, const char* t_a6, const char* t_a7, const char* t_a8, const char* t_a9, const char* t_a10, const char* t_a11);
static t_Flow t_call_decl_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_d, ctron_list* t_vals);
static t_Flow t_call_cv(ctron_list* t_file, ctron_list* t_env, const char* t_out, t_Val t_v, ctron_list* t_vals);
static t_Flow t_call_id(ctron_list* t_file, ctron_list* t_env, const char* t_out, const char* t_nm, ctron_list* t_vals);
static int32_t t_variant_arity(ctron_list* t_file, const char* t_nm);
static t_Val t_vT(const char* t_vt, t_Val t_p);
static t_Val t_conv_as_6(t_Val t_v, const char* t_tgt);
static t_Val t_conv_as(t_Val t_v, const char* t_tgt);
static int32_t t_d_intpart(const char* t_t);
static t_Flow t_ns_map_reduce(ctron_list* t_file, t_Flow t_ob, ctron_list* t_vals, int t_ismap);
static int t_utf8_cont(const char* t_s, int32_t t_k);
static int t_str_contains(const char* t_s, const char* t_sub);
static t_Flow t_call_mem(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_cal, ctron_list* t_vals);
static t_Flow t_eval_call(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e);
static const char* t_bit_p2(int32_t t_k);
static const char* t_bit_quot(const char* t_qr);
static const char* t_bit_rest(const char* t_qr);
static const char* t_bit_umag(const char* t_t, int32_t t_bits);
static const char* t_bit_sfold(const char* t_u, int32_t t_bits, int t_signed);
static const char* t_bit_comb(int32_t t_bits, const char* t_a, const char* t_b, int32_t t_mode);
static int32_t t_bit_width(const char* t_w);
static int t_bit_signed(const char* t_w);
static t_Flow t_bit_ret(const char* t_w, const char* t_u, ctron_list* t_ob);
static t_Flow t_ns_bit(const char* t_m, ctron_list* t_vals, ctron_list* t_ob);
static t_Flow t_run_stmt(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_st);
static t_Flow t_run_block(ctron_list* t_file, ctron_list* t_env0, const char* t_out, ctron_list* t_blk, int t_keep);
static ctron_list* t_statics_env(ctron_list* t_file);
static t_Flow t_run_tests(ctron_list* t_file);
static int t_fmt_kw(const char* t_t);
static int t_fmt_is_digit0(const char* t_t);
static int t_fmt_identk(const char* t_t, const char* t_before);
static int t_fmt_operand_end(const char* t_prev, int t_has_prev, const char* t_prev2);
static int t_fmt_needs_space(const char* t_prev, int t_has_prev, const char* t_prev2, const char* t_cur, int t_pipe_open, int t_sign_unary);
static int32_t t_fmt_free_nl(const char* t_src, ctron_list* t_cstarts, ctron_list* t_cends, int32_t t_from, int32_t t_to);
static int t_fmt_block_ml(const char* t_src, ctron_list* t_tstarts, ctron_list* t_tends, ctron_list* t_partner, int32_t t_i);
static const char* t_fmt_ind(int32_t t_n);
static const char* t_fmt_src(const char* t_src, ctron_list* t_diags);
static int32_t t_main();
int t_or2(int t_a, int t_b) 
{
return (!((!t_a) && (!t_b)));
}
int t_or3(int t_a, int t_b, int t_c) 
{
return t_or2(t_or2(t_a, t_b), t_c);
}
int t_is_digit(int32_t t_b) 
{
return ((t_b >= 48) && (t_b <= 57));
}
int t_is_alpha(int32_t t_b) 
{
return t_or2(((t_b >= 65) && (t_b <= 90)), ((t_b >= 97) && (t_b <= 122)));
}
int t_is_al(int32_t t_b) 
{
return t_or2(t_is_alpha(t_b), (t_b == 95));
}
int t_is_ac(int32_t t_b) 
{
return t_or2(t_is_al(t_b), t_is_digit(t_b));
}
int t_is_hex(int32_t t_b) 
{
return t_or2(((t_b >= 48) && (t_b <= 57)), t_or2(((t_b >= 65) && (t_b <= 70)), ((t_b >= 97) && (t_b <= 102))));
}
int t_is_oct(int32_t t_b) 
{
return ((t_b >= 48) && (t_b <= 55));
}
int t_is_bin(int32_t t_b) 
{
return ((t_b >= 48) && (t_b <= 49));
}
int32_t t_suffix_len(const char* t_src, int32_t t_n, int32_t t_i) 
{
if (((t_i + 2) <= t_n)) {
{
int32_t t_a = ctron_byte_at(t_src, t_i);
int32_t t_b = ctron_byte_at(t_src, (t_i + 1));
int32_t t_len = 0;
if (((t_a == 105) && (t_b == 56))) {
{
t_len = 2;
}
}
if (((((t_a == 105) && (t_b == 49)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 54))) {
{
t_len = 3;
}
}
if (((((t_a == 105) && (t_b == 51)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 50))) {
{
t_len = 3;
}
}
if (((((t_a == 105) && (t_b == 54)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 52))) {
{
t_len = 3;
}
}
if (((((((t_a == 105) && (t_b == 115)) && ((t_i + 5) <= t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 105)) && (ctron_byte_at(t_src, (t_i + 3)) == 122)) && (ctron_byte_at(t_src, (t_i + 4)) == 101))) {
{
t_len = 5;
}
}
if (((t_a == 117) && (t_b == 56))) {
{
t_len = 2;
}
}
if (((((t_a == 117) && (t_b == 49)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 54))) {
{
t_len = 3;
}
}
if (((((t_a == 117) && (t_b == 51)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 50))) {
{
t_len = 3;
}
}
if (((((t_a == 117) && (t_b == 54)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 52))) {
{
t_len = 3;
}
}
if (((((((t_a == 117) && (t_b == 115)) && ((t_i + 5) <= t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 105)) && (ctron_byte_at(t_src, (t_i + 3)) == 122)) && (ctron_byte_at(t_src, (t_i + 4)) == 101))) {
{
t_len = 5;
}
}
if (((((t_a == 102) && (t_b == 51)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 50))) {
{
t_len = 3;
}
}
if (((((t_a == 102) && (t_b == 54)) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 2)) == 52))) {
{
t_len = 3;
}
}
if ((t_len > 0)) {
{
if ((((t_i + t_len) < t_n) && t_is_ac(ctron_byte_at(t_src, (t_i + t_len))))) {
{
return 0;
}
}
return t_len;
}
}
}
}
return 0;
}
int t_nl_set_line_end(const char* t_t) 
{
ctron_list* t_ops = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ops), ",");
ctron_list_push((ctron_list*)(t_ops), "=");
ctron_list_push((ctron_list*)(t_ops), "->");
ctron_list_push((ctron_list*)(t_ops), "=>");
ctron_list_push((ctron_list*)(t_ops), "&&");
ctron_list_push((ctron_list*)(t_ops), "||");
ctron_list_push((ctron_list*)(t_ops), "..");
ctron_list_push((ctron_list*)(t_ops), "..=");
ctron_list_push((ctron_list*)(t_ops), "+");
ctron_list_push((ctron_list*)(t_ops), "-");
ctron_list_push((ctron_list*)(t_ops), "*");
ctron_list_push((ctron_list*)(t_ops), "/");
ctron_list_push((ctron_list*)(t_ops), "%");
ctron_list_push((ctron_list*)(t_ops), "+%");
ctron_list_push((ctron_list*)(t_ops), "-%");
ctron_list_push((ctron_list*)(t_ops), "==");
ctron_list_push((ctron_list*)(t_ops), "!=");
ctron_list_push((ctron_list*)(t_ops), "<");
ctron_list_push((ctron_list*)(t_ops), ">");
ctron_list_push((ctron_list*)(t_ops), "<=");
ctron_list_push((ctron_list*)(t_ops), ">=");
ctron_list_push((ctron_list*)(t_ops), "(");
ctron_list_push((ctron_list*)(t_ops), "[");
ctron_list_push((ctron_list*)(t_ops), "{");
ctron_list_push((ctron_list*)(t_ops), "|");
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_ops))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ops))->items[t_i])), (const char*)(t_t)) == 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
int t_nl_set_next(const char* t_t) 
{
ctron_list* t_ops = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ops), ".");
ctron_list_push((ctron_list*)(t_ops), "+");
ctron_list_push((ctron_list*)(t_ops), "-");
ctron_list_push((ctron_list*)(t_ops), "*");
ctron_list_push((ctron_list*)(t_ops), "/");
ctron_list_push((ctron_list*)(t_ops), "%");
ctron_list_push((ctron_list*)(t_ops), "+%");
ctron_list_push((ctron_list*)(t_ops), "-%");
ctron_list_push((ctron_list*)(t_ops), "==");
ctron_list_push((ctron_list*)(t_ops), "!=");
ctron_list_push((ctron_list*)(t_ops), "<");
ctron_list_push((ctron_list*)(t_ops), ">");
ctron_list_push((ctron_list*)(t_ops), "<=");
ctron_list_push((ctron_list*)(t_ops), ">=");
ctron_list_push((ctron_list*)(t_ops), "&&");
ctron_list_push((ctron_list*)(t_ops), "||");
ctron_list_push((ctron_list*)(t_ops), "..");
ctron_list_push((ctron_list*)(t_ops), "..=");
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_ops))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ops))->items[t_i])), (const char*)(t_t)) == 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
void t_filter_nl(ctron_list* t_raw, ctron_list* t_rlines, ctron_list* t_rcols, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
int32_t t_i = 0;
while (((t_i < ((ctron_list*)(t_raw))->n) && (strcmp((const char*)(((const char*)((ctron_list*)(t_raw))->items[t_i])), (const char*)("NL")) == 0))) {
t_i += 1;
}
while ((t_i < ((ctron_list*)(t_raw))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_raw))->items[t_i])), (const char*)("NL")) != 0)) {
{
ctron_list_push((ctron_list*)(t_toks), ((const char*)((ctron_list*)(t_raw))->items[t_i]));
ctron_list_push((ctron_list*)(t_lns), ((const char*)((ctron_list*)(t_rlines))->items[t_i]));
ctron_list_push((ctron_list*)(t_cols), ((const char*)((ctron_list*)(t_rcols))->items[t_i]));
t_i += 1;
}
}
else {
{
int32_t t_run = t_i;
while (((t_run < ((ctron_list*)(t_raw))->n) && (strcmp((const char*)(((const char*)((ctron_list*)(t_raw))->items[t_run])), (const char*)("NL")) == 0))) {
t_run += 1;
}
const char* t_nxt = "#EOF";
if ((t_run < ((ctron_list*)(t_raw))->n)) {
{
t_nxt = ((const char*)((ctron_list*)(t_raw))->items[t_run]);
}
}
int t_suppressed = 0;
if (((((ctron_list*)(t_toks))->n > 0) && t_nl_set_line_end(((const char*)((ctron_list*)(t_toks))->items[(((ctron_list*)(t_toks))->n - 1)])))) {
{
t_suppressed = 1;
}
}
if (t_nl_set_next(t_nxt)) {
{
t_suppressed = 1;
}
}
if ((!t_suppressed)) {
{
if (((((ctron_list*)(t_toks))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_toks))->items[(((ctron_list*)(t_toks))->n - 1)])), (const char*)(".")) == 0))) {
{
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_rlines))->items[t_i])), (const char*)(":"))), (const char*)(((const char*)((ctron_list*)(t_rcols))->items[t_i])))), (const char*)(" E1001: "))), (const char*)(t_diag_text0("E1001.trailing.dot"))));
}
}
ctron_list_push((ctron_list*)(t_toks), "NL");
ctron_list_push((ctron_list*)(t_lns), ((const char*)((ctron_list*)(t_rlines))->items[t_i]));
ctron_list_push((ctron_list*)(t_cols), ((const char*)((ctron_list*)(t_rcols))->items[t_i]));
}
}
t_i = t_run;
}
}
}
}
void t_pdiag(ctron_list* t_pdiags, ctron_list* t_lns, ctron_list* t_cols, ctron_cell* t_cur, const char* t_code, const char* t_msg) 
{
int32_t t_i = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
const char* t_ln = "0";
if ((t_i < ((ctron_list*)(t_lns))->n)) {
{
t_ln = ((const char*)((ctron_list*)(t_lns))->items[t_i]);
}
}
const char* t_cl = "0";
if ((t_i < ((ctron_list*)(t_cols))->n)) {
{
t_cl = ((const char*)((ctron_list*)(t_cols))->items[t_i]);
}
}
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ln), (const char*)(":"))), (const char*)(t_cl))), (const char*)(" "))), (const char*)(t_code))), (const char*)(": "))), (const char*)(t_msg)));
}
int32_t t_scan5(const char* t_src, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags, ctron_list* t_tstarts, ctron_list* t_tends, ctron_list* t_cstarts, ctron_list* t_cends) 
{
int32_t t_line = 1;
int32_t t_lst = 0;
int32_t t_n = strlen((const char*)(t_src));
int32_t t_i = 0;
{
int32_t t_b = 0;
int32_t t_c0 = 0;
while ((t_i < t_n)) {
t_b = ctron_byte_at(t_src, t_i);
t_c0 = ((t_i - t_lst) + 1);
if ((t_b == 10)) {
{
ctron_list_push((ctron_list*)(t_toks), "NL");
ctron_list_push((ctron_list*)(t_lns), ctron_i32_to_string((int32_t)(t_line)));
ctron_list_push((ctron_list*)(t_cols), ctron_i32_to_string((int32_t)(t_c0)));
ctron_list_push((ctron_list*)(t_tstarts), (const char*)(long)(t_i));
ctron_list_push((ctron_list*)(t_tends), (const char*)(long)((t_i + 1)));
t_line += 1;
t_i += 1;
t_lst = t_i;
}
}
else {
if (t_or2((t_b == 32), t_or2((t_b == 9), (t_b == 13)))) {
{
t_i += 1;
}
}
else {
if ((((t_b == 47) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 47))) {
{
int32_t t_cs = t_i;
while (((t_i < t_n) && (ctron_byte_at(t_src, t_i) != 10))) {
t_i += 1;
}
ctron_list_push((ctron_list*)(t_cstarts), (const char*)(long)(t_cs));
ctron_list_push((ctron_list*)(t_cends), (const char*)(long)(t_i));
}
}
else {
if ((t_b == 34)) {
{
int32_t t_q0 = t_i;
const char* t_err = "";
t_i += 1;
int t_done = 0;
{
int32_t t_c = 0;
while (((t_i < t_n) && (!t_done))) {
t_c = ctron_byte_at(t_src, t_i);
if ((t_c == 34)) {
{
t_i += 1;
t_done = 1;
}
}
else {
if ((t_c == 92)) {
{
if (((t_i + 1) >= t_n)) {
{
t_err = t_diag_text0("E1001.str.unterm");
t_done = 1;
}
}
else {
{
int32_t t_e2 = ctron_byte_at(t_src, (t_i + 1));
if (t_or2((t_e2 == 110), t_or2((t_e2 == 116), t_or2((t_e2 == 114), t_or2((t_e2 == 48), t_or2((t_e2 == 92), t_or2((t_e2 == 34), (t_e2 == 123)))))))) {
{
t_i += 2;
}
}
else {
if ((t_e2 == 117)) {
{
if ((((t_i + 2) < t_n) && (ctron_byte_at(t_src, (t_i + 2)) == 123))) {
{
int32_t t_j = (t_i + 3);
int32_t t_nd = 0;
int32_t t_cp = 0;
int t_ovf = 0;
{
int32_t t_hv = 0;
while (((t_j < t_n) && t_is_hex(ctron_byte_at(t_src, t_j)))) {
t_hv = ctron_byte_at(t_src, t_j);
if ((t_hv <= 57)) {
{
t_hv = (t_hv - 48);
}
}
else {
if ((t_hv <= 70)) {
{
t_hv = (t_hv - 55);
}
}
else {
{
t_hv = (t_hv - 87);
}
}
}
if ((t_cp > ((1114111 - t_hv) / 16))) {
{
t_ovf = 1;
}
}
t_cp = ((t_cp * 16) + t_hv);
t_nd += 1;
t_j += 1;
}
}
if ((((t_j < t_n) && (t_nd > 0)) && (ctron_byte_at(t_src, t_j) == 125))) {
{
t_j += 1;
int t_ill = ((t_ovf || (t_cp > 1114111)) || ((t_cp >= 55296) && (t_cp <= 57343)));
if (t_ill) {
{
t_err = t_diag_text0("E1001.u.illegal");
}
}
t_i = t_j;
}
}
else {
{
t_err = t_diag_text0("E1001.u.nohex");
t_i = t_j;
}
}
}
}
else {
{
t_err = t_diag_text0("E1001.u.nobrace");
t_i += 2;
}
}
}
}
else {
{
t_err = t_diag_text1("E1001.str.esc", ctron_byte_slice(t_src, (t_i + 1), (t_i + 2)));
t_i += 2;
}
}
}
}
}
}
}
else {
if ((t_c == 123)) {
{
int32_t t_depth = 1;
int32_t t_j = (t_i + 1);
int t_term = 0;
{
int32_t t_cj = 0;
while (((t_j < t_n) && (t_depth > 0))) {
t_cj = ctron_byte_at(t_src, t_j);
if ((t_cj == 10)) {
{
t_depth = 0;
}
}
else {
if ((t_cj == 34)) {
{
t_j += 1;
t_depth = 0;
}
}
else {
if ((t_cj == 123)) {
{
t_depth += 1;
t_j += 1;
}
}
else {
if ((t_cj == 125)) {
{
t_depth -= 1;
if ((t_depth == 0)) {
{
t_term = 1;
}
}
else {
{
t_j += 1;
}
}
}
}
else {
{
t_j += 1;
}
}
}
}
}
}
}
if (t_term) {
{
t_i = t_j;
}
}
else {
{
t_err = t_diag_text0("E1001.interp.unterm");
t_done = 1;
t_i = t_j;
}
}
}
}
else {
if ((t_c == 10)) {
{
t_err = t_diag_text0("E1001.str.unterm");
t_done = 1;
}
}
else {
{
t_i += 1;
}
}
}
}
}
}
}
if ((strlen((const char*)(t_err)) > 0)) {
{
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_i32_to_string((int32_t)(t_line))), (const char*)(":"))), (const char*)(ctron_i32_to_string((int32_t)(t_c0))))), (const char*)(" E1001: "))), (const char*)(t_err)));
}
}
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_q0, t_i, ctron_byte_slice(t_src, t_q0, t_i));
}
}
else {
if (t_is_al(t_b)) {
{
int32_t t_start = t_i;
while (((t_i < t_n) && t_is_ac(ctron_byte_at(t_src, t_i)))) {
t_i += 1;
}
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_start, t_i, ctron_byte_slice(t_src, t_start, t_i));
}
}
else {
if (t_is_digit(t_b)) {
{
int32_t t_tstart = t_i;
int32_t t_radix = 0;
if (((t_b == 48) && ((t_i + 1) < t_n))) {
{
int32_t t_p2 = ctron_byte_at(t_src, (t_i + 1));
if ((t_p2 == 120)) {
{
t_radix = 16;
}
}
if ((t_p2 == 111)) {
{
t_radix = 8;
}
}
if ((t_p2 == 98)) {
{
t_radix = 2;
}
}
}
}
int t_fnum = 0;
if ((t_radix > 0)) {
{
t_i += 2;
if ((t_radix == 16)) {
{
while (((t_i < t_n) && t_or2(t_is_hex(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
}
}
if ((t_radix == 8)) {
{
while (((t_i < t_n) && t_or2(t_is_oct(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
}
}
if ((t_radix == 2)) {
{
while (((t_i < t_n) && t_or2(t_is_bin(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
}
}
}
}
else {
{
while (((t_i < t_n) && t_or2(t_is_digit(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
if (((((t_i + 1) < t_n) && (ctron_byte_at(t_src, t_i) == 46)) && t_is_digit(ctron_byte_at(t_src, (t_i + 1))))) {
{
t_fnum = 1;
t_i += 1;
while (((t_i < t_n) && t_or2(t_is_digit(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
}
}
if (((t_i < t_n) && t_or2((ctron_byte_at(t_src, t_i) == 101), (ctron_byte_at(t_src, t_i) == 69)))) {
{
int t_esign = 0;
if ((((t_i + 1) < t_n) && t_or2((ctron_byte_at(t_src, (t_i + 1)) == 43), (ctron_byte_at(t_src, (t_i + 1)) == 45)))) {
{
t_esign = 1;
}
}
int t_eok = 0;
if (((t_esign && ((t_i + 2) < t_n)) && t_is_digit(ctron_byte_at(t_src, (t_i + 2))))) {
{
t_eok = 1;
}
}
if ((((!t_esign) && ((t_i + 1) < t_n)) && t_is_digit(ctron_byte_at(t_src, (t_i + 1))))) {
{
t_eok = 1;
}
}
if (t_eok) {
{
t_fnum = 1;
t_i += 1;
if (t_esign) {
{
t_i += 1;
}
}
while (((t_i < t_n) && t_or2(t_is_digit(ctron_byte_at(t_src, t_i)), (ctron_byte_at(t_src, t_i) == 95)))) {
t_i += 1;
}
}
}
}
}
}
}
int32_t t_tend = t_i;
const char* t_tk = ctron_str_concat((const char*)(ctron_byte_slice(t_src, t_tstart, t_tend)), (const char*)("~"));
int32_t t_sl = t_suffix_len(t_src, t_n, t_i);
if ((t_sl > 0)) {
{
t_tk = ctron_str_concat((const char*)(t_tk), (const char*)(ctron_byte_slice(t_src, t_i, (t_i + t_sl))));
}
}
t_i += t_sl;
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_tstart, t_i, t_tk);
}
}
else {
{
if (((((t_b == 46) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 46)) && (ctron_byte_at(t_src, (t_i + 2)) == 46))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 3), "...");
t_i += 3;
}
}
else {
if (((((t_b == 46) && ((t_i + 2) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 46)) && (ctron_byte_at(t_src, (t_i + 2)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 3), "..=");
t_i += 3;
}
}
else {
if ((((t_b == 46) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 46))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "..");
t_i += 2;
}
}
else {
if ((((t_b == 45) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 62))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "->");
t_i += 2;
}
}
else {
if ((((t_b == 61) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 62))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "=>");
t_i += 2;
}
}
else {
if ((((t_b == 61) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "==");
t_i += 2;
}
}
else {
if ((((t_b == 33) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "!=");
t_i += 2;
}
}
else {
if ((((t_b == 60) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "<=");
t_i += 2;
}
}
else {
if ((((t_b == 62) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), ">=");
t_i += 2;
}
}
else {
if ((((t_b == 38) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 38))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "&&");
t_i += 2;
}
}
else {
if ((((t_b == 124) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 124))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "||");
t_i += 2;
}
}
else {
if ((((t_b == 43) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 37))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "+%");
t_i += 2;
}
}
else {
if ((((t_b == 45) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 37))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "-%");
t_i += 2;
}
}
else {
if ((((t_b == 43) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "+=");
t_i += 2;
}
}
else {
if ((((t_b == 45) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "-=");
t_i += 2;
}
}
else {
if ((((t_b == 42) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "*=");
t_i += 2;
}
}
else {
if ((((t_b == 47) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "/=");
t_i += 2;
}
}
else {
if ((((t_b == 37) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 61))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), "%=");
t_i += 2;
}
}
else {
if ((((t_b == 58) && ((t_i + 1) < t_n)) && (ctron_byte_at(t_src, (t_i + 1)) == 58))) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 2), ":");
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_i32_to_string((int32_t)(t_line))), (const char*)(":"))), (const char*)(ctron_i32_to_string((int32_t)(t_c0))))), (const char*)(" E1001: "))), (const char*)(t_diag_text0("E1001.punct.colon"))));
t_i += 2;
}
}
else {
if ((t_b == 59)) {
{
t_i += 1;
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_i32_to_string((int32_t)(t_line))), (const char*)(":"))), (const char*)(ctron_i32_to_string((int32_t)(t_c0))))), (const char*)(" E1001: "))), (const char*)(t_diag_text0("E1001.punct.semi"))));
}
}
else {
if (t_lex_single(t_b)) {
{
t_rpush(t_toks, t_lns, t_cols, t_tstarts, t_tends, t_line, t_c0, t_i, (t_i + 1), ctron_byte_slice(t_src, t_i, (t_i + 1)));
t_i += 1;
}
}
else {
{
ctron_list_push((ctron_list*)(t_pdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_i32_to_string((int32_t)(t_line))), (const char*)(":"))), (const char*)(ctron_i32_to_string((int32_t)(t_c0))))), (const char*)(" E1001: "))), (const char*)(t_diag_text1("E1001.badchar", ctron_byte_slice(t_src, t_i, (t_i + 1))))));
t_i += 1;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
return t_line;
}
void t_scan4(const char* t_src, ctron_list* t_toks, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
ctron_list* t_raw = (ctron_list*)(ctron_list_new());
ctron_list* t_rlines = (ctron_list*)(ctron_list_new());
ctron_list* t_rcols = (ctron_list*)(ctron_list_new());
ctron_list* t_tstarts = (ctron_list*)(ctron_list_new());
ctron_list* t_tends = (ctron_list*)(ctron_list_new());
ctron_list* t_cstarts = (ctron_list*)(ctron_list_new());
ctron_list* t_cends = (ctron_list*)(ctron_list_new());
int32_t t_line = t_scan5(t_src, t_raw, t_rlines, t_rcols, t_pdiags, t_tstarts, t_tends, t_cstarts, t_cends);
t_filter_nl(t_raw, t_rlines, t_rcols, t_toks, t_lns, t_cols, t_pdiags);
ctron_list_push((ctron_list*)(t_toks), "#EOF");
ctron_list_push((ctron_list*)(t_lns), ctron_i32_to_string((int32_t)(t_line)));
ctron_list_push((ctron_list*)(t_cols), "0");
}
int t_lex_single(int32_t t_b) 
{
const char* t_ks = "+-*/%=<>?!.,:[](){}|&#@";
int32_t t_k = 0;
while ((t_k < strlen((const char*)(t_ks)))) {
if ((ctron_byte_at(t_ks, t_k) == t_b)) {
{
return 1;
}
}
t_k += 1;
}
return 0;
}
void t_scan(const char* t_src, ctron_list* t_toks, ctron_list* t_lns) 
{
ctron_list* t_cols = (ctron_list*)(ctron_list_new());
ctron_list* t_pd = (ctron_list*)(ctron_list_new());
t_scan4(t_src, t_toks, t_lns, t_cols, t_pd);
}
void t_rpush(ctron_list* t_raw, ctron_list* t_rlines, ctron_list* t_rcols, ctron_list* t_tstarts, ctron_list* t_tends, int32_t t_line, int32_t t_col, int32_t t_s, int32_t t_e, const char* t_t) 
{
ctron_list_push((ctron_list*)(t_raw), t_t);
ctron_list_push((ctron_list*)(t_rlines), ctron_i32_to_string((int32_t)(t_line)));
ctron_list_push((ctron_list*)(t_rcols), ctron_i32_to_string((int32_t)(t_col)));
ctron_list_push((ctron_list*)(t_tstarts), (const char*)(long)(t_s));
ctron_list_push((ctron_list*)(t_tends), (const char*)(long)(t_e));
}
const char* t_tok(ctron_list* t_toks, ctron_cell* t_cur) 
{
int32_t t_i = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
if ((t_i < ((ctron_list*)(t_toks))->n)) {
{
return ((const char*)((ctron_list*)(t_toks))->items[t_i]);
}
}
return "#EOF";
}
void t_adv(ctron_cell* t_cur) 
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
const char* t_num_text(const char* t_t) 
{
int32_t t_k = 0;
while (((t_k < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_k) != 126))) {
t_k += 1;
}
return ctron_byte_slice(t_t, 0, t_k);
}
const char* t_num_sfx(const char* t_t) 
{
int32_t t_k = 0;
while (((t_k < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_k) != 126))) {
t_k += 1;
}
return ctron_byte_slice(t_t, (t_k + 1), strlen((const char*)(t_t)));
}
const char* t_diag_lang() 
{
return "ANCHORLANG";
}
const char* t_diag_tpl(const char* t_key) 
{
if (t_seq2(t_diag_lang(), "en")) {
{
const char* t_e = t_diag_tpl_en(t_key);
if ((strlen((const char*)(t_e)) > 0)) {
{
return t_e;
}
}
}
}
return t_diag_tpl_zh(t_key);
}
const char* t_diag_tpl_en(const char* t_key) 
{
if (t_seq2(t_key, "E1001.str.unterm")) {
{
return "unterminated string (no line span)";
}
}
if (t_seq2(t_key, "E1001.str.esc")) {
{
return "illegal escape \\%0";
}
}
if (t_seq2(t_key, "E1001.u.illegal")) {
{
return "illegal Unicode escape";
}
}
if (t_seq2(t_key, "E1001.u.nobrace")) {
{
return "\\u escape missing HEX wrap";
}
}
if (t_seq2(t_key, "E1001.u.nohex")) {
{
return "\\u escape missing HEX digits";
}
}
if (t_seq2(t_key, "E1001.interp.unterm")) {
{
return "unterminated interpolation";
}
}
if (t_seq2(t_key, "E3030")) {
{
return "static var does not exist; use static let or Global[T]:%0";
}
}
if (t_seq2(t_key, "W8010")) {
{
return "struct contains class-reference field (shallow copy):%0.%1";
}
}
if (t_seq2(t_key, "E3031")) {
{
return "static storage is non-Send (Send):%0";
}
}
if (t_seq2(t_key, "W8050")) {
{
return "extern missing #[trusted] (trust boundary):%0";
}
}
if (t_seq2(t_key, "E4041")) {
{
return "#[repr(c)] only allowed on struct declarations:%0";
}
}
if (t_seq2(t_key, "W8052.param")) {
{
return "extern parameter is not a C-ABI type (§9.6):%0.%1";
}
}
if (t_seq2(t_key, "W8052.ret")) {
{
return "extern return type is not a C-ABI type (§9.6):%0";
}
}
if (t_seq2(t_key, "E4040")) {
{
return "#[trusted] only allowed on extern declarations:%0";
}
}
if (t_seq2(t_key, "E4044")) {
{
return "variadic parameter (...) only allowed on extern declarations:%0";
}
}
if (t_seq2(t_key, "W8051")) {
{
return "repr(c) struct has a non-C-ABI field (§9.6):%0.%1";
}
}
if (t_seq2(t_key, "E4050")) {
{
return "class directly holds a resource field (§6.2; resources must be value handles or capability objects):%0.%1";
}
}
if (t_seq2(t_key, "E2040")) {
{
return "integer literal exceeds expected type width (expected %0):%1";
}
}
if (t_seq2(t_key, "E2020.name")) {
{
return "unresolved name (unresolved):%0";
}
}
if (t_seq2(t_key, "E2010.arity")) {
{
return "call argument count mismatch (arity):%0 expected %1 got %2";
}
}
if (t_seq2(t_key, "E2010.arg")) {
{
return "argument type mismatch (type):%0 arg %1 (%2) expected %3 got %4";
}
}
if (t_seq2(t_key, "E2061")) {
{
return "cannot uniquely infer type arguments (candidate conflict); annotate explicitly: %0[T](…)";
}
}
if (t_seq2(t_key, "E2060")) {
{
return "cannot infer type argument `%0` (not in argument position); annotate explicitly, e.g. %1[T](…)";
}
}
if (t_seq2(t_key, "E2010.arith.str")) {
{
return "arithmetic on Str (type):%0";
}
}
if (t_seq2(t_key, "E2010.concat")) {
{
return "Str concatenation requires both sides to be Str (type):+";
}
}
if (t_seq2(t_key, "E2010.arith.bool")) {
{
return "arithmetic on Bool (type):%0";
}
}
if (t_seq2(t_key, "E2010.op.and")) {
{
return "logical AND requires Bool operands (type):&&";
}
}
if (t_seq2(t_key, "E2010.op.or")) {
{
return "logical OR requires Bool operands (type):||";
}
}
if (t_seq2(t_key, "E2010.op.not")) {
{
return "logical NOT requires Bool operand (type):!";
}
}
if (t_seq2(t_key, "E2010.op.neg")) {
{
return "unary minus requires numeric operand (type):-";
}
}
if (t_seq2(t_key, "E2010.prop")) {
{
return "`?` in a function not returning Option/Result (type):%0";
}
}
if (t_seq2(t_key, "E2010.cond.if")) {
{
return "condition must be Bool (type):if";
}
}
if (t_seq2(t_key, "E2010.let")) {
{
return "let initializer type mismatch (type):expected %0 got %1";
}
}
if (t_seq2(t_key, "E2050")) {
{
return "bound not satisfied (bound):%0 param %1 requires %2 got %3";
}
}
if (t_seq2(t_key, "W8040")) {
{
return "shadows a prelude symbol (shadow prelude):%0";
}
}
if (t_seq2(t_key, "E2010.cond.while")) {
{
return "condition must be Bool (type):while";
}
}
if (t_seq2(t_key, "E2010.ret")) {
{
return "return type mismatch (type):expected %0 got %1";
}
}
if (t_seq2(t_key, "E2010.assign")) {
{
return "assignment type mismatch (type):target %0 got %1";
}
}
if (t_seq2(t_key, "W8030")) {
{
return "unused binding (unused):%0";
}
}
if (t_seq2(t_key, "E2072.brk")) {
{
return "break must not cross a closure boundary (closure body is a separate function; cannot break/continue the outer loop)";
}
}
if (t_seq2(t_key, "E2070.brk")) {
{
return "break outside a loop (binds to the nearest loop in the same function body)";
}
}
if (t_seq2(t_key, "E2071.brk")) {
{
return "break crosses a scope holding Drop locals (v1 static rejection; move Drop locals into an inner block or restructure the loop)";
}
}
if (t_seq2(t_key, "E2072.cont")) {
{
return "continue must not cross a closure boundary (closure body is a separate function; cannot break/continue the outer loop)";
}
}
if (t_seq2(t_key, "E2070.cont")) {
{
return "continue outside a loop (binds to the nearest loop in the same function body)";
}
}
if (t_seq2(t_key, "E2071.cont")) {
{
return "continue crosses a scope holding Drop locals (v1 static rejection; move Drop locals into an inner block or restructure the loop)";
}
}
if (t_seq2(t_key, "E6020.effect")) {
{
return "comptime function has a side effect (effect):%0 calls %1";
}
}
if (t_seq2(t_key, "E5010")) {
{
return "orphan impl (orphan):%0 for %1 — neither the trait nor the type belongs to this package";
}
}
if (t_seq2(t_key, "E6010")) {
{
return "comptime budget exceeded (budget):%0";
}
}
if (t_seq2(t_key, "E2010.const")) {
{
return "const type mismatch (type):%0 expected %1 got %2";
}
}
if (t_seq2(t_key, "E2040.const")) {
{
return "const literal exceeds I64 width (range):%0";
}
}
if (t_seq2(t_key, "E4030")) {
{
return "spawn inside a #[no_spawn] context";
}
}
if (t_seq2(t_key, "E3020.elem")) {
{
return "channel element is non-Send (Send):channel element type";
}
}
if (t_seq2(t_key, "E4042")) {
{
return "capturing closure cannot be used as a C-ABI callback (no env slot):%0";
}
}
if (t_seq2(t_key, "E1001.chain")) {
{
return "comparison operators cannot be chained: write a < b && b < c";
}
}
if (t_seq2(t_key, "W8020")) {
{
return "dropped result (must-use):%0 returns Option/Result";
}
}
if (t_seq2(t_key, "E4010")) {
{
return "capability %0 used beyond the manifest (caps) declaration: parameter &%1";
}
}
if (t_seq2(t_key, "E5020.circ")) {
{
return "circular module import (circular):%0";
}
}
if (t_seq2(t_key, "E2020.use.miss")) {
{
return "use import: symbol not found:%0";
}
}
if (t_seq2(t_key, "E2020.use.priv")) {
{
return "use import: symbol is not public (pub):%0";
}
}
if (t_seq2(t_key, "E5030")) {
{
return "use import name collision:%0 (duplicates a merged module/entry; rename or consolidate imports)";
}
}
if (t_seq2(t_key, "E5035")) {
{
return "use alias collision:%0 (alias conflicts with an existing name; pick another)";
}
}
if (t_seq2(t_key, "E2020.use.nat")) {
{
return "use alias on native/prelude symbol:%0 (not supported; import without alias)";
}
}
if (t_seq2(t_key, "E1001.use.alias.ident")) {
{
return "use alias must be an identifier";
}
}
if (t_seq2(t_key, "E1001.use.alias.tail")) {
{
return "expected , or } after use alias";
}
}
if (t_seq2(t_key, "E2020.use.read")) {
{
return "use: module file unreadable:%0";
}
}
if (t_seq2(t_key, "E3050.umove")) {
{
return "use-after-move (moved):arena handle %0";
}
}
if (t_seq2(t_key, "E3040.own")) {
{
return "GC allocation inside an own block (allocation):%0";
}
}
if (t_seq2(t_key, "E3040.noalloc")) {
{
return "GC allocation in a no_alloc context (allocation):%0";
}
}
if (t_seq2(t_key, "E3010")) {
{
return "spawn captures a non-Send value (Send):%0";
}
}
if (t_seq2(t_key, "E2030")) {
{
return "match not exhaustive (not exhaustive):missing variant %0";
}
}
if (t_seq2(t_key, "E3070")) {
{
return "closure mutable capture without an explicit Mutex[T] wrapper:%0 (suggest: use Mutex[T].with_mut)";
}
}
if (t_seq2(t_key, "E4020.cap")) {
{
return "capability call in a #[pure] function (pure)";
}
}
if (t_seq2(t_key, "E6020.cap")) {
{
return "capability call in a comptime function (comptime)";
}
}
if (t_seq2(t_key, "E1001.punct.colon")) {
{
return "disabled punctuation ::";
}
}
if (t_seq2(t_key, "E1001.punct.semi")) {
{
return "disabled punctuation ;";
}
}
if (t_seq2(t_key, "E1001.badchar")) {
{
return "unrecognized character '%0'";
}
}
if (t_seq2(t_key, "E1001.impl.for")) {
{
return "impl missing for";
}
}
if (t_seq2(t_key, "E1001.trailing.dot")) {
{
return "trailing dot at end of line: method chains use leading-dot style (§1.6)";
}
}
if (t_seq2(t_key, "E1001.exptype")) {
{
return "expected type, got %0";
}
}
if (t_seq2(t_key, "E1001.impl.body")) {
{
return "impl body missing {";
}
}
if (t_seq2(t_key, "E1001.abi")) {
{
return "extern ABI must be the \"c\" string";
}
}
if (t_seq2(t_key, "E8110.np")) {
{
return "view prop check failed: %0";
}
}
if (t_seq2(t_key, "E8100.props")) {
{
return "%0";
}
}
return "";
}
const char* t_diag_tpl_zh(const char* t_key) 
{
if (t_seq2(t_key, "E1001.str.unterm")) {
{
return "未终止的字符串(不允许跨行)";
}
}
if (t_seq2(t_key, "E1001.str.esc")) {
{
return "非法转义 \\%0";
}
}
if (t_seq2(t_key, "E1001.u.illegal")) {
{
return "非法的 Unicode 转义";
}
}
if (t_seq2(t_key, "E1001.u.nobrace")) {
{
return "\\u 转义缺少 HEX 包裹";
}
}
if (t_seq2(t_key, "E1001.u.nohex")) {
{
return "\\u 转义缺少 HEX 数字";
}
}
if (t_seq2(t_key, "E1001.interp.unterm")) {
{
return "未终止的插值";
}
}
if (t_seq2(t_key, "E3030")) {
{
return "static var 不存在;用 static let 或 Global[T]:%0";
}
}
if (t_seq2(t_key, "W8010")) {
{
return "struct 含类引用字段(浅拷贝):%0.%1";
}
}
if (t_seq2(t_key, "E3031")) {
{
return "static 存储非 Send(Send):%0";
}
}
if (t_seq2(t_key, "W8050")) {
{
return "extern 未标记 #[trusted](信任边界):%0";
}
}
if (t_seq2(t_key, "E4041")) {
{
return "#[repr(c)] 仅限 struct 声明:%0";
}
}
if (t_seq2(t_key, "W8052.param")) {
{
return "extern 形参非 C-ABI 类型(§9.6):%0.%1";
}
}
if (t_seq2(t_key, "W8052.ret")) {
{
return "extern 返回非 C-ABI 类型(§9.6):%0";
}
}
if (t_seq2(t_key, "E4040")) {
{
return "#[trusted] 仅限 extern 声明:%0";
}
}
if (t_seq2(t_key, "E4044")) {
{
return "变参形参(...)仅限 extern 声明:%0";
}
}
if (t_seq2(t_key, "W8051")) {
{
return "repr(c) struct 含非 C-ABI 字段(§9.6):%0.%1";
}
}
if (t_seq2(t_key, "E4050")) {
{
return "类直接持有资源字段(§6.2;资源须值句柄或能力对象):%0.%1";
}
}
if (t_seq2(t_key, "E2040")) {
{
return "字面量超出期望整数类型宽度(期望 %0):%1";
}
}
if (t_seq2(t_key, "E2020.name")) {
{
return "未解析的名称(unresolved):%0";
}
}
if (t_seq2(t_key, "E2010.arity")) {
{
return "调用实参数不匹配(arity):%0 期望 %1 实得 %2";
}
}
if (t_seq2(t_key, "E2010.arg")) {
{
return "实参类型不匹配(type):%0 第 %1 实参 %2 期望 %3 实得 %4";
}
}
if (t_seq2(t_key, "E2061")) {
{
return "无法唯一推断类型实参(候选冲突);请显式标注 %0[类型](…)";
}
}
if (t_seq2(t_key, "E2060")) {
{
return "无法推断类型实参 `%0`(未出现于实参位);请显式标注,如 %1[类型](…)";
}
}
if (t_seq2(t_key, "E2010.arith.str")) {
{
return "算术运算遇 Str(type):%0";
}
}
if (t_seq2(t_key, "E2010.concat")) {
{
return "Str 拼接需两侧皆 Str(type):+";
}
}
if (t_seq2(t_key, "E2010.arith.bool")) {
{
return "算术运算遇 Bool(type):%0";
}
}
if (t_seq2(t_key, "E2010.op.and")) {
{
return "逻辑与需 Bool 操作数(type):&&";
}
}
if (t_seq2(t_key, "E2010.op.or")) {
{
return "逻辑或需 Bool 操作数(type):||";
}
}
if (t_seq2(t_key, "E2010.op.not")) {
{
return "逻辑非需 Bool 操作数(type):!";
}
}
if (t_seq2(t_key, "E2010.op.neg")) {
{
return "一元负号需数值操作数(type):-";
}
}
if (t_seq2(t_key, "E2010.prop")) {
{
return "? 传播用于非 Option/Result 返回(type):%0";
}
}
if (t_seq2(t_key, "E2010.cond.if")) {
{
return "条件需 Bool(type):if";
}
}
if (t_seq2(t_key, "E2010.let")) {
{
return "let 初始化类型不匹配(type):期望 %0 实得 %1";
}
}
if (t_seq2(t_key, "E2050")) {
{
return "bound 不满足(bound):%0 型参 %1 需 %2 实参 %3";
}
}
if (t_seq2(t_key, "W8040")) {
{
return "遮蔽前奏符号(shadow prelude):%0";
}
}
if (t_seq2(t_key, "E2010.cond.while")) {
{
return "条件需 Bool(type):while";
}
}
if (t_seq2(t_key, "E2010.ret")) {
{
return "返回类型不匹配(type):期望 %0 实得 %1";
}
}
if (t_seq2(t_key, "E2010.assign")) {
{
return "赋值类型不匹配(type):目标 %0 实得 %1";
}
}
if (t_seq2(t_key, "W8030")) {
{
return "未使用绑定(unused):%0";
}
}
if (t_seq2(t_key, "E2072.brk")) {
{
return "break 不得穿越闭包边界(闭包体是独立函数,不可 break/continue 外层循环)";
}
}
if (t_seq2(t_key, "E2070.brk")) {
{
return "break 出现在循环外(绑定同函数体最近循环)";
}
}
if (t_seq2(t_key, "E2071.brk")) {
{
return "break 需越过带 Drop 局部的作用域(v1 静态拒绝;将 Drop 局部移入内层块或重构循环)";
}
}
if (t_seq2(t_key, "E2072.cont")) {
{
return "continue 不得穿越闭包边界(闭包体是独立函数,不可 break/continue 外层循环)";
}
}
if (t_seq2(t_key, "E2070.cont")) {
{
return "continue 出现在循环外(绑定同函数体最近循环)";
}
}
if (t_seq2(t_key, "E2071.cont")) {
{
return "continue 需越过带 Drop 局部的作用域(v1 静态拒绝;将 Drop 局部移入内层块或重构循环)";
}
}
if (t_seq2(t_key, "E6020.effect")) {
{
return "comptime 函数含副作用(effect):%0 调用 %1";
}
}
if (t_seq2(t_key, "E5010")) {
{
return "孤儿 impl(orphan):%0 for %1 的 trait 与类型均不属本包";
}
}
if (t_seq2(t_key, "E6010")) {
{
return "comptime 预算超限(budget):%0";
}
}
if (t_seq2(t_key, "E2010.const")) {
{
return "const 类型不匹配(type):%0 期望 %1 实得 %2";
}
}
if (t_seq2(t_key, "E2040.const")) {
{
return "const 字面量超出 I64 宽度(值域):%0";
}
}
if (t_seq2(t_key, "E4030")) {
{
return "no_spawn 上下文中的 spawn";
}
}
if (t_seq2(t_key, "E3020.elem")) {
{
return "channel 元素非 Send(Send):Channel 元素类型";
}
}
if (t_seq2(t_key, "E4042")) {
{
return "捕获闭包不可作 C-ABI 回调实参(无 env 槽):%0";
}
}
if (t_seq2(t_key, "E1001.chain")) {
{
return "比较运算符不可链:写 a < b && b < c";
}
}
if (t_seq2(t_key, "W8020")) {
{
return "结果被丢弃(must-use):%0 返回 Option/Result";
}
}
if (t_seq2(t_key, "E4010")) {
{
return "使用 %0 能力超出 manifest(caps) 声明:参数 &%1";
}
}
if (t_seq2(t_key, "E5020.circ")) {
{
return "模块循环导入(circular):%0";
}
}
if (t_seq2(t_key, "E2020.use.miss")) {
{
return "use 导入未找到符号:%0";
}
}
if (t_seq2(t_key, "E2020.use.priv")) {
{
return "use 导入的符号未公开(pub):%0";
}
}
if (t_seq2(t_key, "E5030")) {
{
return "use 导入同名 decl:%0(与已合并模块/入口同名;请重命名或收敛导入)";
}
}
if (t_seq2(t_key, "E5035")) {
{
return "use 别名撞名:%0(别名与既有名冲突;请改别名)";
}
}
if (t_seq2(t_key, "E2020.use.nat")) {
{
return "use 前奏/native 符号不支持别名:%0(请去掉 as 别名导入)";
}
}
if (t_seq2(t_key, "E1001.use.alias.ident")) {
{
return "use 别名须为标识符";
}
}
if (t_seq2(t_key, "E1001.use.alias.tail")) {
{
return "use 别名后应为 , 或 }";
}
}
if (t_seq2(t_key, "E2020.use.read")) {
{
return "use 模块文件不可读:%0";
}
}
if (t_seq2(t_key, "E3050.umove")) {
{
return "use-after-move(moved):arena 句柄 %0";
}
}
if (t_seq2(t_key, "E3040.own")) {
{
return "own 块内 GC 分配(allocation):%0";
}
}
if (t_seq2(t_key, "E3040.noalloc")) {
{
return "no_alloc 上下文 GC 分配(allocation):%0";
}
}
if (t_seq2(t_key, "E3010")) {
{
return "spawn 捕获非 Send(Send):%0";
}
}
if (t_seq2(t_key, "E2030")) {
{
return "match 不穷尽(not exhaustive):缺变体 %0";
}
}
if (t_seq2(t_key, "E3070")) {
{
return "闭包可变捕获未显式 Mutex[T] 包装:%0(建议:改用 Mutex[T].with_mut)";
}
}
if (t_seq2(t_key, "E1001.punct.colon")) {
{
return "禁用的标点 ::";
}
}
if (t_seq2(t_key, "E1001.punct.semi")) {
{
return "禁用的标点 ;";
}
}
if (t_seq2(t_key, "E1001.badchar")) {
{
return "无法识别的字符 '%0'";
}
}
if (t_seq2(t_key, "E1001.impl.for")) {
{
return "impl 缺少 for";
}
}
if (t_seq2(t_key, "E1001.trailing.dot")) {
{
return "行尾点非法:方法链统一首点式(§1.6)";
}
}
if (t_seq2(t_key, "E1001.exptype")) {
{
return "预期类型,实际 %0";
}
}
if (t_seq2(t_key, "E1001.impl.body")) {
{
return "impl 体缺少 {";
}
}
if (t_seq2(t_key, "E1001.abi")) {
{
return "extern ABI 应为 \"c\" 字符串";
}
}
if (t_seq2(t_key, "E4020.cap")) {
{
return "pure 函数含能力调用(pure)";
}
}
if (t_seq2(t_key, "E6020.cap")) {
{
return "comptime 函数含能力调用(comptime)";
}
}
if (t_seq2(t_key, "E8110.np")) {
{
return "组件 prop 核对失败:%0";
}
}
if (t_seq2(t_key, "E8100.props")) {
{
return "%0";
}
}
return "";
}
const char* t_diag_subst(const char* t_tpl, ctron_list* t_args) 
{
const char* t_out = "";
int32_t t_i = 0;
int32_t t_n = strlen((const char*)(t_tpl));
{
int32_t t_c = 0;
while ((t_i < t_n)) {
t_c = ctron_byte_at(t_tpl, t_i);
if (((t_c == 37) && ((t_i + 1) < t_n))) {
{
int32_t t_d = ctron_byte_at(t_tpl, (t_i + 1));
if (((t_d >= 48) && (t_d <= 57))) {
{
int32_t t_k = (t_d - 48);
if ((t_k < ((ctron_list*)(t_args))->n)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_args))->items[t_k])));
}
}
t_i += 2;
continue;
}
}
if ((t_d == 37)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("%"));
t_i += 2;
continue;
}
}
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_tpl, t_i, (t_i + 1))));
t_i += 1;
}
}
return t_out;
}
const char* t_diag_code(const char* t_key) 
{
int32_t t_i = 0;
int32_t t_n = strlen((const char*)(t_key));
while ((t_i < t_n)) {
if ((ctron_byte_at(t_key, t_i) == 46)) {
{
return ctron_byte_slice(t_key, 0, t_i);
}
}
t_i += 1;
}
return t_key;
}
const char* t_diag_text(const char* t_key, ctron_list* t_args) 
{
return t_diag_subst(t_diag_tpl(t_key), t_args);
}
const char* t_diag_text0(const char* t_key) 
{
return t_diag_text(t_key, ctron_list_new());
}
const char* t_diag_text1(const char* t_key, const char* t_a0) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
return t_diag_text(t_key, t_a);
}
const char* t_diag_render(const char* t_key, const char* t_line, ctron_list* t_args) 
{
const char* t_msg = t_diag_subst(t_diag_tpl(t_key), t_args);
const char* t_code = t_diag_code(t_key);
if ((strlen((const char*)(t_line)) > 0)) {
{
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_code), (const char*)("@"))), (const char*)(t_line))), (const char*)(": "))), (const char*)(t_msg));
}
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_code), (const char*)(": "))), (const char*)(t_msg));
}
const char* t_diag_render0(const char* t_key, const char* t_line) 
{
return t_diag_render(t_key, t_line, ctron_list_new());
}
const char* t_diag_render1(const char* t_key, const char* t_line, const char* t_a0) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
return t_diag_render(t_key, t_line, t_a);
}
void t_diag_emit(ctron_list* t_diags, const char* t_key, const char* t_line, ctron_list* t_args) 
{
ctron_list_push((ctron_list*)(t_diags), t_diag_render(t_key, t_line, t_args));
}
void t_diag0(ctron_list* t_diags, const char* t_key, const char* t_line) 
{
t_diag_emit(t_diags, t_key, t_line, ctron_list_new());
}
void t_diag1(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
t_diag_emit(t_diags, t_key, t_line, t_a);
}
void t_diag2(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
t_diag_emit(t_diags, t_key, t_line, t_a);
}
void t_diag3(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
ctron_list_push((ctron_list*)(t_a), t_a2);
t_diag_emit(t_diags, t_key, t_line, t_a);
}
void t_diag4(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2, const char* t_a3) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
ctron_list_push((ctron_list*)(t_a), t_a2);
ctron_list_push((ctron_list*)(t_a), t_a3);
t_diag_emit(t_diags, t_key, t_line, t_a);
}
void t_diag5(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2, const char* t_a3, const char* t_a4) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
ctron_list_push((ctron_list*)(t_a), t_a2);
ctron_list_push((ctron_list*)(t_a), t_a3);
ctron_list_push((ctron_list*)(t_a), t_a4);
t_diag_emit(t_diags, t_key, t_line, t_a);
}
void t_diag_once(ctron_list* t_diags, const char* t_key, const char* t_line, ctron_list* t_args) 
{
const char* t_s = t_diag_render(t_key, t_line, t_args);
if ((!t_in_list(t_diags, t_s))) {
{
ctron_list_push((ctron_list*)(t_diags), t_s);
}
}
}
void t_donce0(ctron_list* t_diags, const char* t_key, const char* t_line) 
{
t_diag_once(t_diags, t_key, t_line, ctron_list_new());
}
void t_donce1(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
t_diag_once(t_diags, t_key, t_line, t_a);
}
void t_donce2(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
t_diag_once(t_diags, t_key, t_line, t_a);
}
void t_donce3(ctron_list* t_diags, const char* t_key, const char* t_line, const char* t_a0, const char* t_a1, const char* t_a2) 
{
ctron_list* t_a = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_a), t_a0);
ctron_list_push((ctron_list*)(t_a), t_a1);
ctron_list_push((ctron_list*)(t_a), t_a2);
t_diag_once(t_diags, t_key, t_line, t_a);
}
ctron_list* t_mk(const char* t_t) 
{
ctron_list* t_n = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_n), t_t);
return t_n;
}
void t_nstamp(ctron_list* t_n, const char* t_line) 
{
ctron_list_push((ctron_list*)(t_n), t_line);
}
void t_ast_put(ctron_list* t_k, ctron_list* t_v, const char* t_key, const char* t_val) 
{
ctron_list_push((ctron_list*)(t_k), t_key);
ctron_list_push((ctron_list*)(t_v), t_val);
}
const char* t_ast_shape(ctron_list* t_k, ctron_list* t_v, const char* t_tag) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_k))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_k))->items[t_i])), (const char*)(t_tag)) == 0)) {
{
return ((const char*)((ctron_list*)(t_v))->items[t_i]);
}
}
t_i += 1;
}
return "?";
}
const char* t_ast_kind(const char* t_sh, int32_t t_si) 
{
if ((strlen((const char*)(t_sh)) == 0)) {
{
return "?";
}
}
int t_star = (strcmp((const char*)(ctron_byte_slice(t_sh, (strlen((const char*)(t_sh)) - 1), strlen((const char*)(t_sh)))), (const char*)("*")) == 0);
int32_t t_n = strlen((const char*)(t_sh));
if (t_star) {
{
t_n = (t_n - 1);
}
}
if ((t_n == 0)) {
{
return "?";
}
}
int32_t t_idx = t_si;
if ((t_idx >= t_n)) {
{
if ((!t_star)) {
{
return "?";
}
}
t_idx = (t_n - 1);
}
}
return ctron_byte_slice(t_sh, t_idx, (t_idx + 1));
}
void t_ast_tables(ctron_list* t_ktags, ctron_list* t_kshapes) 
{
t_ast_put(t_ktags, t_kshapes, "File", "N*");
t_ast_put(t_ktags, t_kshapes, "Expr", "N");
t_ast_put(t_ktags, t_kshapes, "None", "");
t_ast_put(t_ktags, t_kshapes, "Fn", "SNNNNS*");
t_ast_put(t_ktags, t_kshapes, "FnPub", "SNNNNS*");
t_ast_put(t_ktags, t_kshapes, "Method", "SNNNNS*");
t_ast_put(t_ktags, t_kshapes, "FnExt", "SNNNNS*");
t_ast_put(t_ktags, t_kshapes, "FnC", "SNNNNS*");
t_ast_put(t_ktags, t_kshapes, "Struct", "SNNNS*");
t_ast_put(t_ktags, t_kshapes, "Enum", "SNNS*");
t_ast_put(t_ktags, t_kshapes, "Trait", "SNNN");
t_ast_put(t_ktags, t_kshapes, "Impl", "NNNN");
t_ast_put(t_ktags, t_kshapes, "Const", "SNN");
t_ast_put(t_ktags, t_kshapes, "Use", "NN");
t_ast_put(t_ktags, t_kshapes, "Test", "SN");
t_ast_put(t_ktags, t_kshapes, "Ps", "N*");
t_ast_put(t_ktags, t_kshapes, "Param", "SSN");
t_ast_put(t_ktags, t_kshapes, "VaArgs", "");
t_ast_put(t_ktags, t_kshapes, "Fields", "N*");
t_ast_put(t_ktags, t_kshapes, "Field", "SSNS*");
t_ast_put(t_ktags, t_kshapes, "Drvs", "S*");
t_ast_put(t_ktags, t_kshapes, "TPs", "N*");
t_ast_put(t_ktags, t_kshapes, "TPar", "SSN");
t_ast_put(t_ktags, t_kshapes, "Bounds", "S*");
t_ast_put(t_ktags, t_kshapes, "Vars", "N*");
t_ast_put(t_ktags, t_kshapes, "Variant", "SN");
t_ast_put(t_ktags, t_kshapes, "KUnit", "N*");
t_ast_put(t_ktags, t_kshapes, "KTuple", "N*");
t_ast_put(t_ktags, t_kshapes, "KStruct", "N*");
t_ast_put(t_ktags, t_kshapes, "Items", "N*");
t_ast_put(t_ktags, t_kshapes, "Segs", "S*");
t_ast_put(t_ktags, t_kshapes, "Syms", "S*");
t_ast_put(t_ktags, t_kshapes, "Named", "SN");
t_ast_put(t_ktags, t_kshapes, "TArgs", "N*");
t_ast_put(t_ktags, t_kshapes, "TArgs2", "N*");
t_ast_put(t_ktags, t_kshapes, "Ref", "N");
t_ast_put(t_ktags, t_kshapes, "Optional", "N");
t_ast_put(t_ktags, t_kshapes, "TupleT", "N*");
t_ast_put(t_ktags, t_kshapes, "Slice", "N");
t_ast_put(t_ktags, t_kshapes, "ArrayT", "NN");
t_ast_put(t_ktags, t_kshapes, "FnType", "NN");
t_ast_put(t_ktags, t_kshapes, "FnT", "N*");
t_ast_put(t_ktags, t_kshapes, "Block", "N*");
t_ast_put(t_ktags, t_kshapes, "BlockExpr", "N");
t_ast_put(t_ktags, t_kshapes, "Let", "SNNNS");
t_ast_put(t_ktags, t_kshapes, "Return", "NS*");
t_ast_put(t_ktags, t_kshapes, "If", "NNN");
t_ast_put(t_ktags, t_kshapes, "While", "NN");
t_ast_put(t_ktags, t_kshapes, "Match", "NNS");
t_ast_put(t_ktags, t_kshapes, "Arms", "N*");
t_ast_put(t_ktags, t_kshapes, "Arm", "NNNS*");
t_ast_put(t_ktags, t_kshapes, "Call", "NNS*");
t_ast_put(t_ktags, t_kshapes, "Args", "N*");
t_ast_put(t_ktags, t_kshapes, "Unary", "SN");
t_ast_put(t_ktags, t_kshapes, "Binary", "SNNS*");
t_ast_put(t_ktags, t_kshapes, "Ident", "S");
t_ast_put(t_ktags, t_kshapes, "Int", "SS*");
t_ast_put(t_ktags, t_kshapes, "Str", "N");
t_ast_put(t_ktags, t_kshapes, "Parts", "N*");
t_ast_put(t_ktags, t_kshapes, "Text", "S");
t_ast_put(t_ktags, t_kshapes, "PatWild", "");
t_ast_put(t_ktags, t_kshapes, "PatId", "S");
t_ast_put(t_ktags, t_kshapes, "PatLitB", "S");
t_ast_put(t_ktags, t_kshapes, "PatTup", "N*");
t_ast_put(t_ktags, t_kshapes, "PatAgg", "SN");
t_ast_put(t_ktags, t_kshapes, "SubTup", "N*");
t_ast_put(t_ktags, t_kshapes, "SubSt", "N*");
t_ast_put(t_ktags, t_kshapes, "SubUnit", "");
t_ast_put(t_ktags, t_kshapes, "PatFld", "SN");
t_ast_put(t_ktags, t_kshapes, "TypeArgs", "NN");
t_ast_put(t_ktags, t_kshapes, "Index", "NN");
t_ast_put(t_ktags, t_kshapes, "Try", "NS*");
t_ast_put(t_ktags, t_kshapes, "Closure", "NNNS*");
t_ast_put(t_ktags, t_kshapes, "CPs", "N*");
t_ast_put(t_ktags, t_kshapes, "TupleE", "N*");
t_ast_put(t_ktags, t_kshapes, "StructLit", "SNS");
t_ast_put(t_ktags, t_kshapes, "LitFs", "N*");
t_ast_put(t_ktags, t_kshapes, "Bool", "S");
t_ast_put(t_ktags, t_kshapes, "Range", "SNN");
t_ast_put(t_ktags, t_kshapes, "ComptimeVal", "S");
t_ast_put(t_ktags, t_kshapes, "Float", "SS*");
t_ast_put(t_ktags, t_kshapes, "Own", "SN");
t_ast_put(t_ktags, t_kshapes, "ClosureParam", "SSN");
t_ast_put(t_ktags, t_kshapes, "Scope", "SN");
t_ast_put(t_ktags, t_kshapes, "ArrLit", "N*");
t_ast_put(t_ktags, t_kshapes, "Class", "SNN");
t_ast_put(t_ktags, t_kshapes, "LField", "SN");
t_ast_put(t_ktags, t_kshapes, "Interp", "S");
t_ast_put(t_ktags, t_kshapes, "PatLitI", "S");
t_ast_put(t_ktags, t_kshapes, "PatLitF", "S");
t_ast_put(t_ktags, t_kshapes, "PatLitS", "S");
t_ast_put(t_ktags, t_kshapes, "Continue", "S");
t_ast_put(t_ktags, t_kshapes, "Break", "S");
t_ast_put(t_ktags, t_kshapes, "PropSig", "SNN");
t_ast_put(t_ktags, t_kshapes, "PropImpl", "SNN");
t_ast_put(t_ktags, t_kshapes, "Prop", "SNN");
t_ast_put(t_ktags, t_kshapes, "Static", "SSNNS*");
t_ast_put(t_ktags, t_kshapes, "Member", "NSSS*");
t_ast_put(t_ktags, t_kshapes, "For", "NNN");
t_ast_put(t_ktags, t_kshapes, "Assign", "NSNS*");
}
const char* t_qtext(const char* t_raw) 
{
const char* t_out = "";
int32_t t_j = 0;
int32_t t_n = strlen((const char*)(t_raw));
{
int32_t t_c = 0;
while ((t_j < t_n)) {
t_c = ctron_byte_at(t_raw, t_j);
if ((((t_c == 92) && ((t_j + 1) < t_n)) && (ctron_byte_at(t_raw, (t_j + 1)) == 117))) {
{
const char* t_hx = "";
int32_t t_cp = 0;
int32_t t_pp = (t_j + 2);
int t_mapped = 0;
if (((t_pp < t_n) && (ctron_byte_at(t_raw, t_pp) == 123))) {
{
t_pp += 1;
{
int32_t t_hc = 0;
while (((t_pp < t_n) && (ctron_byte_at(t_raw, t_pp) != 125))) {
t_hc = ctron_byte_at(t_raw, t_pp);
if (t_is_hex(t_hc)) {
{
t_hx = ctron_str_concat((const char*)(t_hx), (const char*)(ctron_byte_slice(t_raw, t_pp, (t_pp + 1))));
t_cp = ((t_cp * 16) + t_digit_of(t_hc));
}
}
t_pp += 1;
}
}
if ((strlen((const char*)(t_hx)) > 0)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_utf8_enc((int)(t_cp))));
t_mapped = 1;
}
}
}
}
if (t_mapped) {
{
t_j = (t_pp + 1);
}
}
else {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_raw, t_j, (t_j + 2))));
t_j += 2;
}
}
}
}
else {
if ((((t_c == 92) && ((t_j + 1) < t_n)) && (ctron_byte_at(t_raw, (t_j + 1)) == 123))) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("{"));
t_j += 2;
}
}
else {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_raw, t_j, (t_j + 1))));
t_j += 1;
}
}
}
}
}
return t_out;
}
ctron_list* t_parts_of(const char* t_raw) 
{
ctron_list* t_np = (ctron_list*)(t_mk("Parts"));
int32_t t_k = 0;
int32_t t_n = strlen((const char*)(t_raw));
int32_t t_j = 0;
int32_t t_ts = 0;
{
int32_t t_c = 0;
while ((t_j < t_n)) {
t_c = ctron_byte_at(t_raw, t_j);
if (((t_c == 92) && ((t_j + 2) <= t_n))) {
{
if (((((t_j + 3) < t_n) && (ctron_byte_at(t_raw, (t_j + 1)) == 117)) && (ctron_byte_at(t_raw, (t_j + 2)) == 123))) {
{
int32_t t_q = (t_j + 3);
while (((t_q < t_n) && (ctron_byte_at(t_raw, t_q) != 125))) {
t_q += 1;
}
if ((t_q < t_n)) {
{
t_j = (t_q + 1);
}
}
else {
{
t_j += 2;
}
}
}
}
else {
{
t_j += 2;
}
}
}
}
else {
if ((t_c == 123)) {
{
if ((t_j > t_ts)) {
{
ctron_list* t_tx = (ctron_list*)(t_mk("Text"));
ctron_list_push((ctron_list*)(t_tx), t_qtext(ctron_byte_slice(t_raw, t_ts, t_j)));
ctron_list_push((ctron_list*)(t_np), (char*)(t_tx));
t_k += 1;
}
}
int32_t t_d = 1;
int32_t t_p = (t_j + 1);
{
int32_t t_cp = 0;
while (((t_d > 0) && (t_p < t_n))) {
t_cp = ctron_byte_at(t_raw, t_p);
if ((t_cp == 123)) {
{
t_d += 1;
}
}
if ((t_cp == 125)) {
{
t_d -= 1;
}
}
t_p += 1;
}
}
ctron_list* t_ix = (ctron_list*)(t_mk("Interp"));
ctron_list_push((ctron_list*)(t_ix), t_qtext(ctron_byte_slice(t_raw, (t_j + 1), (t_p - 1))));
ctron_list_push((ctron_list*)(t_np), (char*)(t_ix));
t_k += 1;
t_j = t_p;
t_ts = t_p;
}
}
else {
{
t_j += 1;
}
}
}
}
}
if ((t_n > t_ts)) {
{
ctron_list* t_ty = (ctron_list*)(t_mk("Text"));
ctron_list_push((ctron_list*)(t_ty), t_qtext(ctron_byte_slice(t_raw, t_ts, t_n)));
ctron_list_push((ctron_list*)(t_np), (char*)(t_ty));
t_k += 1;
}
}
return t_np;
}
ctron_list* t_p_oror(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_or(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
while (t_go) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("||")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_or(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), "OrOr");
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
else {
{
t_go = 0;
}
}
}
return t_l;
}
ctron_list* t_p_and(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_cmp(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
while (t_go) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("&&")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_cmp(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), "AndAnd");
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
else {
{
t_go = 0;
}
}
}
return t_l;
}
ctron_list* t_p_cmp(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_rng(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
{
const char* t_t = 0;
const char* t_op = 0;
while (t_go) {
t_t = t_tok(t_toks, t_cur);
t_op = "";
if ((strcmp((const char*)(t_t), (const char*)("==")) == 0)) {
{
t_op = "Eq";
}
}
if ((strcmp((const char*)(t_t), (const char*)("!=")) == 0)) {
{
t_op = "Ne";
}
}
if ((strcmp((const char*)(t_t), (const char*)("<")) == 0)) {
{
t_op = "Lt";
}
}
if ((strcmp((const char*)(t_t), (const char*)(">")) == 0)) {
{
t_op = "Gt";
}
}
if ((strcmp((const char*)(t_t), (const char*)("<=")) == 0)) {
{
t_op = "Le";
}
}
if ((strcmp((const char*)(t_t), (const char*)(">=")) == 0)) {
{
t_op = "Ge";
}
}
if ((strcmp((const char*)(t_op), (const char*)("")) == 0)) {
{
t_go = 0;
}
}
else {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_rng(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), t_op);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
}
}
return t_l;
}
ctron_list* t_p_rng(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_f = (ctron_list*)(t_p_add(t_toks, t_cur, t_lns, t_al));
const char* t_t = t_tok(t_toks, t_cur);
if (t_or2((strcmp((const char*)(t_t), (const char*)("..")) == 0), (strcmp((const char*)(t_t), (const char*)("..=")) == 0))) {
{
t_adv(t_cur);
ctron_list* t_to = (ctron_list*)(t_p_add(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Range"));
if ((strcmp((const char*)(t_t), (const char*)("..=")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nb), "true");
}
}
else {
{
ctron_list_push((ctron_list*)(t_nb), "false");
}
}
ctron_list_push((ctron_list*)(t_nb), (char*)(t_f));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_to));
return t_nb;
}
}
return t_f;
}
ctron_list* t_p_add(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_mul(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
{
const char* t_t = 0;
const char* t_op = 0;
while (t_go) {
t_t = t_tok(t_toks, t_cur);
t_op = "";
if ((strcmp((const char*)(t_t), (const char*)("+")) == 0)) {
{
t_op = "Add";
}
}
if ((strcmp((const char*)(t_t), (const char*)("-")) == 0)) {
{
t_op = "Sub";
}
}
if ((strcmp((const char*)(t_t), (const char*)("+%")) == 0)) {
{
t_op = "WrapAdd";
}
}
if ((strcmp((const char*)(t_t), (const char*)("-%")) == 0)) {
{
t_op = "WrapSub";
}
}
if ((strcmp((const char*)(t_op), (const char*)("")) == 0)) {
{
t_go = 0;
}
}
else {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_mul(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), t_op);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
}
}
return t_l;
}
ctron_list* t_p_mul(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_una(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
{
const char* t_t = 0;
const char* t_op = 0;
while (t_go) {
t_t = t_tok(t_toks, t_cur);
t_op = "";
if ((strcmp((const char*)(t_t), (const char*)("*")) == 0)) {
{
t_op = "Mul";
}
}
if ((strcmp((const char*)(t_t), (const char*)("/")) == 0)) {
{
t_op = "Div";
}
}
if ((strcmp((const char*)(t_t), (const char*)("%")) == 0)) {
{
t_op = "Mod";
}
}
if ((strcmp((const char*)(t_op), (const char*)("")) == 0)) {
{
t_go = 0;
}
}
else {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_una(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), t_op);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
}
}
return t_l;
}
ctron_list* t_p_una(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
const char* t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("-")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_e = (ctron_list*)(t_p_una(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nu = (ctron_list*)(t_mk("Unary"));
ctron_list_push((ctron_list*)(t_nu), "Neg");
ctron_list_push((ctron_list*)(t_nu), (char*)(t_e));
return t_nu;
}
}
if ((strcmp((const char*)(t_t), (const char*)("!")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_e = (ctron_list*)(t_p_una(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nu = (ctron_list*)(t_mk("Unary"));
ctron_list_push((ctron_list*)(t_nu), "Not");
ctron_list_push((ctron_list*)(t_nu), (char*)(t_e));
return t_nu;
}
}
return t_p_post(t_toks, t_cur, t_lns, t_al);
}
ctron_list* t_p_post(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_e = (ctron_list*)(t_p_pri(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
{
const char* t_t = 0;
const char* t_ln0 = 0;
while (t_go) {
while (((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)])), (const char*)(".")) == 0))) {
t_adv(t_cur);
}
t_t = t_tok(t_toks, t_cur);
t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_t), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_ag = (ctron_list*)(t_mk("Args"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list* t_a0 = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a0))->items[0])), (const char*)("Ident")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_np = (ctron_list*)(t_mk("NParg"));
ctron_list_push((ctron_list*)(t_np), ((const char*)((ctron_list*)(t_a0))->items[1]));
ctron_list_push((ctron_list*)(t_np), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_np, t_ln0);
ctron_list_push((ctron_list*)(t_ag), (char*)(t_np));
}
}
else {
{
ctron_list_push((ctron_list*)(t_ag), (char*)(t_a0));
}
}
}
}
else {
{
ctron_list_push((ctron_list*)(t_ag), (char*)(t_a0));
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
ctron_list* t_cl = (ctron_list*)(t_mk("Call"));
ctron_list_push((ctron_list*)(t_cl), (char*)(t_e));
ctron_list_push((ctron_list*)(t_cl), (char*)(t_ag));
t_nstamp(t_cl, t_ln0);
t_e = (ctron_list*)(t_cl);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)(".")) == 0)) {
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_mb = (ctron_list*)(t_mk("Member"));
ctron_list_push((ctron_list*)(t_mb), (char*)(t_e));
const char* t_mbline = t_ln0;
if (t_is_digit(ctron_byte_at(t_nm, 0))) {
{
ctron_list_push((ctron_list*)(t_mb), "TIdx");
ctron_list_push((ctron_list*)(t_mb), t_num_text(t_nm));
}
}
else {
{
ctron_list_push((ctron_list*)(t_mb), "Nm");
ctron_list_push((ctron_list*)(t_mb), t_nm);
}
}
t_nstamp(t_mb, t_mbline);
t_e = (ctron_list*)(t_mb);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("[")) == 0)) {
{
int32_t t_d = 1;
int32_t t_q = (__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1);
int t_hascom = 0;
{
const char* t_tk2 = 0;
while ((t_d > 0)) {
t_tk2 = ((const char*)((ctron_list*)(t_toks))->items[t_q]);
if ((strcmp((const char*)(t_tk2), (const char*)("[")) == 0)) {
{
t_d += 1;
}
}
if ((strcmp((const char*)(t_tk2), (const char*)("]")) == 0)) {
{
t_d -= 1;
}
}
if (((strcmp((const char*)(t_tk2), (const char*)(",")) == 0) && (t_d == 1))) {
{
t_hascom = 1;
}
}
t_q += 1;
}
}
const char* t_nx = ((const char*)((ctron_list*)(t_toks))->items[t_q]);
int t_ta = 0;
if ((strcmp((const char*)(t_nx), (const char*)("(")) == 0)) {
{
t_ta = 1;
}
}
if ((t_or2((strcmp((const char*)(t_al), (const char*)("1")) == 0), 0) && (strcmp((const char*)(t_nx), (const char*)("{")) == 0))) {
{
t_ta = 1;
}
}
if (((strcmp((const char*)(t_nx), (const char*)(".")) == 0) && t_hascom)) {
{
t_ta = 1;
}
}
if (t_ta) {
{
t_adv(t_cur);
ctron_list* t_ag = (ctron_list*)(t_mk("TArgs2"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_more = 0;
}
}
else {
{
const char* t_at = t_tok(t_toks, t_cur);
if (t_is_digit(ctron_byte_at(t_at, 0))) {
{
ctron_list* t_cv = (ctron_list*)(t_mk("ComptimeVal"));
ctron_list_push((ctron_list*)(t_cv), t_num_text(t_at));
ctron_list_push((ctron_list*)(t_ag), (char*)(t_cv));
t_adv(t_cur);
}
}
else {
{
ctron_list_push((ctron_list*)(t_ag), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
ctron_list* t_tg = (ctron_list*)(t_mk("TypeArgs"));
ctron_list_push((ctron_list*)(t_tg), (char*)(t_e));
ctron_list_push((ctron_list*)(t_tg), (char*)(t_ag));
t_e = (ctron_list*)(t_tg);
}
}
else {
{
t_adv(t_cur);
ctron_list* t_ix = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1"));
t_adv(t_cur);
ctron_list* t_inode = (ctron_list*)(t_mk("Index"));
ctron_list_push((ctron_list*)(t_inode), (char*)(t_e));
ctron_list_push((ctron_list*)(t_inode), (char*)(t_ix));
t_e = (ctron_list*)(t_inode);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("?")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_tr = (ctron_list*)(t_mk("Try"));
ctron_list_push((ctron_list*)(t_tr), (char*)(t_e));
t_nstamp(t_tr, t_ln0);
t_e = (ctron_list*)(t_tr);
}
}
else {
{
t_go = 0;
}
}
}
}
}
}
}
return t_e;
}
ctron_list* t_p_pri(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
const char* t_t = t_tok(t_toks, t_cur);
int32_t t_b0 = ctron_byte_at(t_t, 0);
if (t_is_digit(t_b0)) {
{
t_adv(t_cur);
ctron_list* t_nb = (ctron_list*)(t_mk("Int"));
int t_isf = 0;
const char* t_tx = t_num_text(t_t);
int t_hexint = 0;
if ((strlen((const char*)(t_tx)) > 1)) {
{
if ((ctron_byte_at(t_tx, 0) == 48)) {
{
if ((ctron_byte_at(t_tx, 1) == 120)) {
{
t_hexint = 1;
}
}
}
}
}
}
int32_t t_jj = 0;
{
int32_t t_cc = 0;
while ((t_jj < strlen((const char*)(t_tx)))) {
t_cc = ctron_byte_at(t_tx, t_jj);
if ((t_cc == 46)) {
{
t_isf = 1;
}
}
if ((!t_hexint)) {
{
if (t_or2((t_cc == 101), (t_cc == 69))) {
{
t_isf = 1;
}
}
}
}
t_jj += 1;
}
}
if (t_isf) {
{
t_nb = (ctron_list*)(t_mk("Float"));
}
}
ctron_list_push((ctron_list*)(t_nb), t_num_text(t_t));
ctron_list_push((ctron_list*)(t_nb), t_num_sfx(t_t));
return t_nb;
}
}
if ((strcmp((const char*)(t_t), (const char*)("if")) == 0)) {
{
return t_p_if(t_toks, t_cur, t_lns);
}
}
if ((strcmp((const char*)(t_t), (const char*)("match")) == 0)) {
{
return t_p_match(t_toks, t_cur, t_lns);
}
}
if ((strcmp((const char*)(t_t), (const char*)("void")) == 0)) {
{
t_adv(t_cur);
return t_mk("Void");
}
}
if ((strcmp((const char*)(t_t), (const char*)("true")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_bt = (ctron_list*)(t_mk("Bool"));
ctron_list_push((ctron_list*)(t_bt), "true");
return t_bt;
}
}
if ((strcmp((const char*)(t_t), (const char*)("false")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_bf = (ctron_list*)(t_mk("Bool"));
ctron_list_push((ctron_list*)(t_bf), "false");
return t_bf;
}
}
if ((strcmp((const char*)(t_t), (const char*)("own")) == 0)) {
{
t_adv(t_cur);
t_adv(t_cur);
const char* t_an = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_ow = (ctron_list*)(t_mk("Own"));
ctron_list_push((ctron_list*)(t_ow), t_an);
ctron_list_push((ctron_list*)(t_ow), (char*)(t_p_block(t_toks, t_cur, t_lns)));
return t_ow;
}
}
if ((strcmp((const char*)(t_t), (const char*)("||")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_cl = (ctron_list*)(t_mk("Closure"));
const char* t_cln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
ctron_list_push((ctron_list*)(t_cl), (char*)(t_mk("CPs")));
ctron_list* t_ret0 = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("->")) == 0)) {
{
t_adv(t_cur);
t_ret0 = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list_push((ctron_list*)(t_cl), (char*)(t_ret0));
ctron_list_push((ctron_list*)(t_cl), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_cl, t_cln0);
return t_cl;
}
}
if ((strcmp((const char*)(t_t), (const char*)("|")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_cl = (ctron_list*)(t_mk("Closure"));
const char* t_cln = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
ctron_list* t_cp = (ctron_list*)(t_mk("CPs"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("|")) == 0)) {
{
t_more = 0;
}
}
else {
{
const char* t_isv = "false";
const char* t_t0 = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t0), (const char*)("var")) == 0)) {
{
t_isv = "true";
}
}
if (t_or2((strcmp((const char*)(t_t0), (const char*)("var")) == 0), (strcmp((const char*)(t_t0), (const char*)("let")) == 0))) {
{
t_adv(t_cur);
}
}
const char* t_cn = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_cpar = (ctron_list*)(t_mk("ClosureParam"));
ctron_list_push((ctron_list*)(t_cpar), t_isv);
ctron_list_push((ctron_list*)(t_cpar), t_cn);
ctron_list* t_ty = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
t_ty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list_push((ctron_list*)(t_cpar), (char*)(t_ty));
ctron_list_push((ctron_list*)(t_cp), (char*)(t_cpar));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
t_adv(t_cur);
ctron_list* t_ret = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("->")) == 0)) {
{
t_adv(t_cur);
t_ret = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list_push((ctron_list*)(t_cl), (char*)(t_cp));
ctron_list_push((ctron_list*)(t_cl), (char*)(t_ret));
ctron_list_push((ctron_list*)(t_cl), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_cl, t_cln);
return t_cl;
}
}
if ((strcmp((const char*)(t_t), (const char*)("scope")) == 0)) {
{
t_adv(t_cur);
t_adv(t_cur);
t_adv(t_cur);
const char* t_pn = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_sc = (ctron_list*)(t_mk("Scope"));
ctron_list_push((ctron_list*)(t_sc), t_pn);
ctron_list_push((ctron_list*)(t_sc), (char*)(t_p_block(t_toks, t_cur, t_lns)));
return t_sc;
}
}
if ((t_b0 == 34)) {
{
t_adv(t_cur);
ctron_list* t_ns = (ctron_list*)(t_mk("Str"));
ctron_list_push((ctron_list*)(t_ns), (char*)(t_parts_of(ctron_byte_slice(t_t, 1, (strlen((const char*)(t_t)) - 1)))));
return t_ns;
}
}
if ((strcmp((const char*)(t_t), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_be = (ctron_list*)(t_mk("BlockExpr"));
ctron_list_push((ctron_list*)(t_be), (char*)(t_p_block(t_toks, t_cur, t_lns)));
return t_be;
}
}
if ((strcmp((const char*)(t_t), (const char*)("[")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_al = (ctron_list*)(t_mk("ArrLit"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_al), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
return t_al;
}
}
if ((strcmp((const char*)(t_t), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_e0 = (ctron_list*)(t_p_oror(t_toks, t_cur, t_lns, "1"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
ctron_list* t_tn = (ctron_list*)(t_mk("TupleE"));
ctron_list_push((ctron_list*)(t_tn), (char*)(t_e0));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_tn), (char*)(t_p_oror(t_toks, t_cur, t_lns, "1")));
}
}
else {
{
t_more = 0;
}
}
}
t_adv(t_cur);
return t_tn;
}
}
t_adv(t_cur);
return t_e0;
}
}
if (((((strcmp((const char*)(t_al), (const char*)("1")) == 0) && (t_b0 >= 65)) && (t_b0 <= 90)) && (strcmp((const char*)(((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)])), (const char*)("{")) == 0))) {
{
const char* t_nm = t_t;
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_sl = (ctron_list*)(t_mk("StructLit"));
ctron_list_push((ctron_list*)(t_sl), t_nm);
ctron_list* t_lf = (ctron_list*)(t_mk("LitFs"));
int t_more = 1;
while (t_more) {
if (t_or2((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0), (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) == 0))) {
{
t_more = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
const char* t_fnm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_fv = (ctron_list*)(t_mk("LField"));
ctron_list_push((ctron_list*)(t_fv), t_fnm);
ctron_list_push((ctron_list*)(t_fv), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
ctron_list_push((ctron_list*)(t_lf), (char*)(t_fv));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_sl), (char*)(t_lf));
t_nstamp(t_sl, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
return t_sl;
}
}
t_adv(t_cur);
ctron_list* t_ni = (ctron_list*)(t_mk("Ident"));
ctron_list_push((ctron_list*)(t_ni), t_t);
return t_ni;
}
ctron_list* t_p_tpars(ctron_list* t_toks, ctron_cell* t_cur) 
{
t_adv(t_cur);
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_more = 0;
}
}
else {
{
const char* t_isc = "false";
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("comptime")) == 0)) {
{
t_isc = "true";
t_adv(t_cur);
}
}
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_tp1 = (ctron_list*)(t_mk("TPar"));
ctron_list_push((ctron_list*)(t_tp1), t_nm);
ctron_list_push((ctron_list*)(t_tp1), t_isc);
ctron_list* t_bd = (ctron_list*)(t_mk("Bounds"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
int t_more2 = 1;
while (t_more2) {
ctron_list_push((ctron_list*)(t_bd), t_tok(t_toks, t_cur));
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("+")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more2 = 0;
}
}
}
}
}
ctron_list_push((ctron_list*)(t_tp1), (char*)(t_bd));
ctron_list_push((ctron_list*)(t_tp), (char*)(t_tp1));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
return t_tp;
}
ctron_list* t_p_typ(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
const char* t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_tt = (ctron_list*)(t_mk("TupleT"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_tt), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
return t_tt;
}
}
if ((strcmp((const char*)(t_t), (const char*)("&")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_rf = (ctron_list*)(t_mk("Ref"));
ctron_list_push((ctron_list*)(t_rf), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("?")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_op = (ctron_list*)(t_mk("Optional"));
ctron_list_push((ctron_list*)(t_op), (char*)(t_rf));
return t_op;
}
}
return t_rf;
}
}
if ((strcmp((const char*)(t_t), (const char*)("fn")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_ft = (ctron_list*)(t_mk("FnT"));
t_adv(t_cur);
int t_more2 = 1;
while (t_more2) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more2 = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_ft), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more2 = 0;
}
}
}
}
}
t_adv(t_cur);
ctron_list* t_fret = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("->")) == 0)) {
{
t_adv(t_cur);
t_fret = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list* t_ftn = (ctron_list*)(t_mk("FnType"));
ctron_list_push((ctron_list*)(t_ftn), (char*)(t_ft));
ctron_list_push((ctron_list*)(t_ftn), (char*)(t_fret));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("?")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_op2 = (ctron_list*)(t_mk("Optional"));
ctron_list_push((ctron_list*)(t_op2), (char*)(t_ftn));
return t_op2;
}
}
return t_ftn;
}
}
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_nt = (ctron_list*)(t_mk("Named"));
ctron_list_push((ctron_list*)(t_nt), t_nm);
ctron_list* t_ta = (ctron_list*)(t_mk("TArgs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
const char* t_nx1 = ((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
int t_isempty = 0;
int t_single = 0;
if ((strcmp((const char*)(t_nx1), (const char*)("]")) == 0)) {
{
t_isempty = 1;
}
}
if (((((!t_isempty) && (strcmp((const char*)(t_nx1), (const char*)("#EOF")) != 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 2)])), (const char*)("]")) == 0)) && t_is_digit(ctron_byte_at(t_nx1, 0)))) {
{
t_single = 1;
}
}
if (((!t_isempty) && (!t_single))) {
{
t_adv(t_cur);
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_ta), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
}
}
}
}
ctron_list_push((ctron_list*)(t_nt), (char*)(t_ta));
ctron_list* t_base = (ctron_list*)(t_nt);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
const char* t_nx2 = ((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
int t_isarr = 0;
if ((strcmp((const char*)(t_nx2), (const char*)("]")) == 0)) {
{
t_isarr = 1;
}
}
if ((((strcmp((const char*)(t_nx2), (const char*)("#EOF")) != 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_toks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 2)])), (const char*)("]")) == 0)) && t_is_digit(ctron_byte_at(t_nx2, 0)))) {
{
t_isarr = 1;
}
}
if (t_isarr) {
{
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_sl = (ctron_list*)(t_mk("Slice"));
ctron_list_push((ctron_list*)(t_sl), (char*)(t_base));
t_base = (ctron_list*)(t_sl);
}
}
else {
{
ctron_list* t_sz = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1"));
t_adv(t_cur);
ctron_list* t_ar = (ctron_list*)(t_mk("ArrayT"));
ctron_list_push((ctron_list*)(t_ar), (char*)(t_base));
ctron_list_push((ctron_list*)(t_ar), (char*)(t_sz));
t_base = (ctron_list*)(t_ar);
}
}
}
}
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("?")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_op3 = (ctron_list*)(t_mk("Optional"));
ctron_list_push((ctron_list*)(t_op3), (char*)(t_base));
t_base = (ctron_list*)(t_op3);
}
}
return t_base;
}
ctron_list* t_p_or(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
ctron_list* t_l = (ctron_list*)(t_p_and(t_toks, t_cur, t_lns, t_al));
int t_go = 1;
while (t_go) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("or")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_r = (ctron_list*)(t_p_and(t_toks, t_cur, t_lns, t_al));
ctron_list* t_nb = (ctron_list*)(t_mk("Binary"));
ctron_list_push((ctron_list*)(t_nb), "Or");
ctron_list_push((ctron_list*)(t_nb), (char*)(t_l));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_r));
t_nstamp(t_nb, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
t_l = (ctron_list*)(t_nb);
}
}
else {
{
t_go = 0;
}
}
}
return t_l;
}
ctron_list* t_p_stmt_expr(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_al) 
{
return t_p_oror(t_toks, t_cur, t_lns, t_al);
}
ctron_list* t_p_pattern_or(ctron_list* t_toks, ctron_cell* t_cur) 
{
ctron_list* t_first = (ctron_list*)(t_p_pattern(t_toks, t_cur));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("|")) != 0)) {
{
return t_first;
}
}
ctron_list* t_po = (ctron_list*)(t_mk("PatOr"));
ctron_list_push((ctron_list*)(t_po), (char*)(t_first));
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("|")) == 0)) {
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_po), (char*)(t_p_pattern(t_toks, t_cur)));
}
return t_po;
}
ctron_list* t_p_pattern(ctron_list* t_toks, ctron_cell* t_cur) 
{
const char* t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("_")) == 0)) {
{
t_adv(t_cur);
return t_mk("PatWild");
}
}
int32_t t_b0 = ctron_byte_at(t_t, 0);
if (t_is_digit(t_b0)) {
{
t_adv(t_cur);
int t_isf = 0;
const char* t_tx = t_num_text(t_t);
int32_t t_jj = 0;
{
int32_t t_cc = 0;
while ((t_jj < strlen((const char*)(t_tx)))) {
t_cc = ctron_byte_at(t_tx, t_jj);
if (t_or2((t_cc == 46), t_or2((t_cc == 101), (t_cc == 69)))) {
{
t_isf = 1;
}
}
t_jj += 1;
}
}
ctron_list* t_np = (ctron_list*)(t_mk("PatLitI"));
if (t_isf) {
{
t_np = (ctron_list*)(t_mk("PatLitF"));
}
}
ctron_list_push((ctron_list*)(t_np), t_num_text(t_t));
return t_np;
}
}
if ((t_b0 == 34)) {
{
t_adv(t_cur);
ctron_list* t_np = (ctron_list*)(t_mk("PatLitS"));
ctron_list_push((ctron_list*)(t_np), t_qtext(ctron_byte_slice(t_t, 1, (strlen((const char*)(t_t)) - 1))));
return t_np;
}
}
if ((strcmp((const char*)(t_t), (const char*)("true")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_np = (ctron_list*)(t_mk("PatLitB"));
ctron_list_push((ctron_list*)(t_np), "true");
return t_np;
}
}
if ((strcmp((const char*)(t_t), (const char*)("false")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_np = (ctron_list*)(t_mk("PatLitB"));
ctron_list_push((ctron_list*)(t_np), "false");
return t_np;
}
}
if ((strcmp((const char*)(t_t), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_np = (ctron_list*)(t_mk("PatTup"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_np), (char*)(t_p_pattern(t_toks, t_cur)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
return t_np;
}
}
const char* t_nm = t_t;
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_agg = (ctron_list*)(t_mk("PatAgg"));
ctron_list_push((ctron_list*)(t_agg), t_nm);
ctron_list* t_sub = (ctron_list*)(t_mk("SubTup"));
int t_more2 = 1;
while (t_more2) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more2 = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_sub), (char*)(t_p_pattern(t_toks, t_cur)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more2 = 0;
}
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_agg), (char*)(t_sub));
return t_agg;
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_agg = (ctron_list*)(t_mk("PatAgg"));
ctron_list_push((ctron_list*)(t_agg), t_nm);
ctron_list* t_sub = (ctron_list*)(t_mk("SubSt"));
int t_more3 = 1;
while (t_more3) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_more3 = 0;
}
}
else {
{
const char* t_fnm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_pp = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
t_pp = (ctron_list*)(t_p_pattern(t_toks, t_cur));
}
}
ctron_list* t_fld = (ctron_list*)(t_mk("PatFld"));
ctron_list_push((ctron_list*)(t_fld), t_fnm);
ctron_list_push((ctron_list*)(t_fld), (char*)(t_pp));
ctron_list_push((ctron_list*)(t_sub), (char*)(t_fld));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_agg), (char*)(t_sub));
return t_agg;
}
}
if ((t_or2((t_b0 >= 65), 0) && (t_b0 <= 90))) {
{
ctron_list* t_agg = (ctron_list*)(t_mk("PatAgg"));
ctron_list_push((ctron_list*)(t_agg), t_nm);
ctron_list_push((ctron_list*)(t_agg), (char*)(t_mk("SubUnit")));
return t_agg;
}
}
ctron_list* t_pi = (ctron_list*)(t_mk("PatId"));
ctron_list_push((ctron_list*)(t_pi), t_nm);
return t_pi;
}
ctron_list* t_p_match(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
const char* t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
t_adv(t_cur);
ctron_list* t_nm = (ctron_list*)(t_mk("Match"));
ctron_list_push((ctron_list*)(t_nm), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "0")));
t_adv(t_cur);
ctron_list* t_arms = (ctron_list*)(t_mk("Arms"));
int t_go = 1;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) == 0)) {
{
t_go = 0;
}
}
else {
{
ctron_list* t_ma = (ctron_list*)(t_mk("Arm"));
ctron_list_push((ctron_list*)(t_ma), (char*)(t_p_pattern_or(t_toks, t_cur)));
int t_hasg = 0;
ctron_list* t_gd = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("if")) == 0)) {
{
t_adv(t_cur);
t_gd = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1"));
t_hasg = 1;
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ma), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
if (t_hasg) {
{
ctron_list_push((ctron_list*)(t_ma), (char*)(t_gd));
}
}
ctron_list_push((ctron_list*)(t_arms), (char*)(t_ma));
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nm), (char*)(t_arms));
t_nstamp(t_nm, t_ln0);
return t_nm;
}
ctron_list* t_p_if(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
t_adv(t_cur);
ctron_list* t_ni = (ctron_list*)(t_mk("If"));
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_oror(t_toks, t_cur, t_lns, "0")));
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_block(t_toks, t_cur, t_lns)));
ctron_list* t_els = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("else")) == 0)) {
{
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("if")) == 0)) {
{
t_els = (ctron_list*)(t_p_if(t_toks, t_cur, t_lns));
}
}
else {
{
t_adv(t_cur);
ctron_list* t_be = (ctron_list*)(t_mk("BlockExpr"));
ctron_list_push((ctron_list*)(t_be), (char*)(t_p_block(t_toks, t_cur, t_lns)));
t_els = (ctron_list*)(t_be);
}
}
}
}
ctron_list_push((ctron_list*)(t_ni), (char*)(t_els));
return t_ni;
}
ctron_list* t_p_block(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
ctron_list* t_nb = (ctron_list*)(t_mk("Block"));
int t_tailSet = 0;
int t_go = 1;
{
int32_t t_p0 = 0;
const char* t_t = 0;
const char* t_ln0 = 0;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_p0 = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
t_t = t_tok(t_toks, t_cur);
t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_t), (const char*)("}")) == 0)) {
{
t_adv(t_cur);
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("#EOF")) == 0)) {
{
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("return")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_re = (ctron_list*)(t_mk("Return"));
const char* t_reline = t_ln0;
const char* t_rt = t_tok(t_toks, t_cur);
if (t_or3((strcmp((const char*)(t_rt), (const char*)("}")) == 0), (strcmp((const char*)(t_rt), (const char*)("#EOF")) == 0), (strcmp((const char*)(t_rt), (const char*)("NL")) == 0))) {
{
ctron_list* t_rn = (ctron_list*)(t_mk("None"));
ctron_list_push((ctron_list*)(t_re), (char*)(t_rn));
}
}
else {
{
ctron_list_push((ctron_list*)(t_re), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
}
}
t_nstamp(t_re, t_reline);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_re));
}
}
else {
if (t_or2((strcmp((const char*)(t_t), (const char*)("let")) == 0), (strcmp((const char*)(t_t), (const char*)("var")) == 0))) {
{
const char* t_isv = "false";
if ((strcmp((const char*)(t_t), (const char*)("var")) == 0)) {
{
t_isv = "true";
}
}
t_adv(t_cur);
ctron_list* t_pp = (ctron_list*)(t_p_pattern(t_toks, t_cur));
ctron_list* t_ty = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
t_ty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
t_adv(t_cur);
ctron_list* t_lv = (ctron_list*)(t_mk("Let"));
ctron_list_push((ctron_list*)(t_lv), t_isv);
const char* t_lvline = t_ln0;
ctron_list_push((ctron_list*)(t_lv), (char*)(t_pp));
ctron_list_push((ctron_list*)(t_lv), (char*)(t_ty));
ctron_list_push((ctron_list*)(t_lv), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_lv, t_lvline);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_lv));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("while")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_wc = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "0"));
t_adv(t_cur);
ctron_list* t_wb = (ctron_list*)(t_p_block(t_toks, t_cur, t_lns));
ctron_list* t_wh = (ctron_list*)(t_mk("While"));
ctron_list_push((ctron_list*)(t_wh), (char*)(t_wc));
ctron_list_push((ctron_list*)(t_wh), (char*)(t_wb));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_wh));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("for")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_fp = (ctron_list*)(t_p_pattern(t_toks, t_cur));
t_adv(t_cur);
ctron_list* t_it = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "0"));
t_adv(t_cur);
ctron_list* t_fb = (ctron_list*)(t_p_block(t_toks, t_cur, t_lns));
ctron_list* t_fo = (ctron_list*)(t_mk("For"));
ctron_list_push((ctron_list*)(t_fo), (char*)(t_fp));
ctron_list_push((ctron_list*)(t_fo), (char*)(t_it));
ctron_list_push((ctron_list*)(t_fo), (char*)(t_fb));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_fo));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("break")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_bk = (ctron_list*)(t_mk("Break"));
t_nstamp(t_bk, t_ln0);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_bk));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("continue")) == 0)) {
{
t_adv(t_cur);
ctron_list* t_cn = (ctron_list*)(t_mk("Continue"));
t_nstamp(t_cn, t_ln0);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_cn));
}
}
else {
{
ctron_list* t_e = (ctron_list*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1"));
const char* t_t2 = t_tok(t_toks, t_cur);
const char* t_op = "";
if ((strcmp((const char*)(t_t2), (const char*)("=")) == 0)) {
{
t_op = "Eq";
}
}
if ((strcmp((const char*)(t_t2), (const char*)("+=")) == 0)) {
{
t_op = "AddEq";
}
}
if ((strcmp((const char*)(t_t2), (const char*)("-=")) == 0)) {
{
t_op = "SubEq";
}
}
if ((strcmp((const char*)(t_t2), (const char*)("*=")) == 0)) {
{
t_op = "MulEq";
}
}
if ((strcmp((const char*)(t_t2), (const char*)("/=")) == 0)) {
{
t_op = "DivEq";
}
}
if ((strcmp((const char*)(t_t2), (const char*)("%=")) == 0)) {
{
t_op = "ModEq";
}
}
if ((strcmp((const char*)(t_op), (const char*)("")) != 0)) {
{
t_adv(t_cur);
ctron_list* t_asn = (ctron_list*)(t_mk("Assign"));
ctron_list_push((ctron_list*)(t_asn), (char*)(t_e));
const char* t_asline = t_ln0;
ctron_list_push((ctron_list*)(t_asn), t_op);
ctron_list_push((ctron_list*)(t_asn), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_asn, t_asline);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_asn));
}
}
else {
{
const char* t_nx = t_t2;
if ((strcmp((const char*)(t_nx), (const char*)("NL")) == 0)) {
{
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_nx = t_tok(t_toks, t_cur);
}
}
if ((strcmp((const char*)(t_nx), (const char*)("}")) == 0)) {
{
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_e));
t_tailSet = 1;
t_go = 0;
}
}
else {
{
ctron_list* t_es = (ctron_list*)(t_mk("Expr"));
ctron_list_push((ctron_list*)(t_es), (char*)(t_e));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_es));
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) == t_p0)) {
{
t_adv(t_cur);
}
}
}
}
if ((!t_tailSet)) {
{
ctron_list* t_nn = (ctron_list*)(t_mk("None"));
ctron_list_push((ctron_list*)(t_nb), (char*)(t_nn));
}
}
return t_nb;
}
ctron_list* t_p_fn2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_tag) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_nf = (ctron_list*)(t_mk(t_tag));
ctron_list_push((ctron_list*)(t_nf), t_nm);
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
t_tp = (ctron_list*)(t_p_tpars(t_toks, t_cur));
}
}
t_adv(t_cur);
ctron_list* t_ps = (ctron_list*)(t_mk("Ps"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("...")) == 0)) {
{
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ps), (char*)(t_mk("VaArgs")));
t_more = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more = 0;
}
}
else {
{
const char* t_isv = "false";
const char* t_t0 = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t0), (const char*)("var")) == 0)) {
{
t_isv = "true";
}
}
if (t_or2((strcmp((const char*)(t_t0), (const char*)("var")) == 0), (strcmp((const char*)(t_t0), (const char*)("let")) == 0))) {
{
t_adv(t_cur);
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("&")) == 0)) {
{
t_adv(t_cur);
if (t_or2((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("var")) == 0), (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("let")) == 0))) {
{
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("var")) == 0)) {
{
t_isv = "true";
}
}
t_adv(t_cur);
}
}
}
}
const char* t_pn = t_tok(t_toks, t_cur);
t_adv(t_cur);
if ((strcmp((const char*)(t_pn), (const char*)("self")) == 0)) {
{
ctron_list* t_pty = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
t_pty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list* t_par = (ctron_list*)(t_mk("Param"));
ctron_list_push((ctron_list*)(t_par), t_isv);
ctron_list_push((ctron_list*)(t_par), t_pn);
ctron_list_push((ctron_list*)(t_par), (char*)(t_pty));
ctron_list_push((ctron_list*)(t_ps), (char*)(t_par));
}
}
else {
{
t_adv(t_cur);
ctron_list* t_pty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
ctron_list* t_par = (ctron_list*)(t_mk("Param"));
ctron_list_push((ctron_list*)(t_par), t_isv);
ctron_list_push((ctron_list*)(t_par), t_pn);
ctron_list_push((ctron_list*)(t_par), (char*)(t_pty));
ctron_list_push((ctron_list*)(t_ps), (char*)(t_par));
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
}
t_adv(t_cur);
ctron_list* t_ret = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("->")) == 0)) {
{
t_adv(t_cur);
t_ret = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
}
}
ctron_list* t_body = (ctron_list*)(t_mk("None"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
t_body = (ctron_list*)(t_p_block(t_toks, t_cur, t_lns));
}
}
ctron_list_push((ctron_list*)(t_nf), (char*)(t_tp));
ctron_list_push((ctron_list*)(t_nf), (char*)(t_ps));
ctron_list_push((ctron_list*)(t_nf), (char*)(t_ret));
ctron_list_push((ctron_list*)(t_nf), (char*)(t_body));
return t_nf;
}
ctron_list* t_p_fn(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
return t_p_fn2(t_toks, t_cur, t_lns, "Fn");
}
ctron_list* t_p_test(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
t_adv(t_cur);
const char* t_t = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_nt = (ctron_list*)(t_mk("Test"));
ctron_list_push((ctron_list*)(t_nt), t_qtext(ctron_byte_slice(t_t, 1, (strlen((const char*)(t_t)) - 1))));
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nt), (char*)(t_p_block(t_toks, t_cur, t_lns)));
return t_nt;
}
ctron_list* t_p_field(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
const char* t_isv = "false";
const char* t_t = t_tok(t_toks, t_cur);
const char* t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_t), (const char*)("var")) == 0)) {
{
t_isv = "true";
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("let")) == 0), (strcmp((const char*)(t_t), (const char*)("var")) == 0))) {
{
t_adv(t_cur);
}
}
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_f = (ctron_list*)(t_mk("Field"));
ctron_list_push((ctron_list*)(t_f), t_isv);
ctron_list_push((ctron_list*)(t_f), t_nm);
ctron_list_push((ctron_list*)(t_f), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
t_nstamp(t_f, t_ln0);
return t_f;
}
ctron_list* t_p_struct2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_dr, const char* t_anm) 
{
const char* t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
t_tp = (ctron_list*)(t_p_tpars(t_toks, t_cur));
}
}
t_adv(t_cur);
ctron_list* t_ns = (ctron_list*)(t_mk("Struct"));
ctron_list_push((ctron_list*)(t_ns), t_nm);
ctron_list_push((ctron_list*)(t_ns), (char*)(t_dr));
ctron_list_push((ctron_list*)(t_ns), (char*)(t_tp));
ctron_list* t_fs = (ctron_list*)(t_mk("Fields"));
int t_go = 1;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) == 0)) {
{
t_go = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_fs), (char*)(t_p_field(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ns), (char*)(t_fs));
if ((strcmp((const char*)(t_anm), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_ns), t_anm);
}
}
t_nstamp(t_ns, t_ln0);
return t_ns;
}
ctron_list* t_p_struct(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
ctron_list* t_dr0 = (ctron_list*)(t_mk("Drvs"));
return t_p_struct2(t_toks, t_cur, t_lns, t_dr0, "");
}
ctron_list* t_p_enum2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_dr, const char* t_anm) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_ne = (ctron_list*)(t_mk("Enum"));
ctron_list_push((ctron_list*)(t_ne), t_nm);
ctron_list_push((ctron_list*)(t_ne), (char*)(t_dr));
ctron_list* t_vs = (ctron_list*)(t_mk("Vars"));
int t_go = 1;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) == 0)) {
{
t_go = 0;
}
}
else {
{
const char* t_vn = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_v = (ctron_list*)(t_mk("Variant"));
ctron_list_push((ctron_list*)(t_v), t_vn);
ctron_list* t_kind = (ctron_list*)(t_mk("KUnit"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
t_kind = (ctron_list*)(t_mk("KTuple"));
int t_more = 1;
while (t_more) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_kind), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
t_adv(t_cur);
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
t_kind = (ctron_list*)(t_mk("KStruct"));
int t_more2 = 1;
while (t_more2) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_more2 = 0;
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) == 0)) {
{
t_more2 = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_kind), (char*)(t_p_field(t_toks, t_cur, t_lns)));
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
}
t_adv(t_cur);
}
}
}
ctron_list_push((ctron_list*)(t_v), (char*)(t_kind));
ctron_list_push((ctron_list*)(t_vs), (char*)(t_v));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ne), (char*)(t_vs));
if ((strcmp((const char*)(t_anm), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_ne), t_anm);
}
}
return t_ne;
}
ctron_list* t_p_prop_item2(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_kind) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_ty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
ctron_list* t_body = (ctron_list*)(t_mk("None"));
const char* t_tag = "PropSig";
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
t_body = (ctron_list*)(t_p_block(t_toks, t_cur, t_lns));
t_tag = "PropImpl";
}
}
if ((strcmp((const char*)(t_kind), (const char*)("imp")) == 0)) {
{
t_tag = "Prop";
}
}
ctron_list* t_pn = (ctron_list*)(t_mk(t_tag));
ctron_list_push((ctron_list*)(t_pn), t_nm);
ctron_list_push((ctron_list*)(t_pn), (char*)(t_ty));
ctron_list_push((ctron_list*)(t_pn), (char*)(t_body));
return t_pn;
}
ctron_list* t_p_items(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, const char* t_kind) 
{
ctron_list* t_it = (ctron_list*)(t_mk("Items"));
int t_go = 1;
{
const char* t_t = 0;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_t = t_tok(t_toks, t_cur);
if (t_or2((strcmp((const char*)(t_t), (const char*)("}")) == 0), (strcmp((const char*)(t_t), (const char*)("#EOF")) == 0))) {
{
t_adv(t_cur);
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("#")) == 0)) {
{
t_adv(t_cur);
t_adv(t_cur);
const char* t_anm = t_tok(t_toks, t_cur);
t_adv(t_cur);
while (((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) != 0) && (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) != 0))) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_adv(t_cur);
}
}
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
ctron_list* t_md = (ctron_list*)(t_p_fn2(t_toks, t_cur, t_lns, "Method"));
ctron_list_push((ctron_list*)(t_md), t_anm);
ctron_list_push((ctron_list*)(t_it), (char*)(t_md));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("fn")) == 0)) {
{
ctron_list_push((ctron_list*)(t_it), (char*)(t_p_fn2(t_toks, t_cur, t_lns, "Method")));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("prop")) == 0)) {
{
const char* t_pk = "sig";
if (t_or2((strcmp((const char*)(t_kind), (const char*)("impl")) == 0), (strcmp((const char*)(t_kind), (const char*)("class")) == 0))) {
{
t_pk = "imp";
}
}
ctron_list_push((ctron_list*)(t_it), (char*)(t_p_prop_item2(t_toks, t_cur, t_lns, t_pk)));
}
}
else {
if ((strcmp((const char*)(t_kind), (const char*)("trait")) == 0)) {
{
t_go = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_it), (char*)(t_p_field(t_toks, t_cur, t_lns)));
}
}
}
}
}
}
}
}
t_adv(t_cur);
return t_it;
}
ctron_list* t_p_class(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_nc = (ctron_list*)(t_mk("Class"));
ctron_list_push((ctron_list*)(t_nc), t_nm);
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
t_tp = (ctron_list*)(t_p_tpars(t_toks, t_cur));
}
}
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nc), (char*)(t_tp));
ctron_list_push((ctron_list*)(t_nc), (char*)(t_p_items(t_toks, t_cur, t_lns, "class")));
return t_nc;
}
ctron_list* t_p_impl(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
t_adv(t_cur);
ctron_list* t_ni = (ctron_list*)(t_mk("Impl"));
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
t_tp = (ctron_list*)(t_p_tpars(t_toks, t_cur));
}
}
ctron_list_push((ctron_list*)(t_ni), (char*)(t_tp));
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("for")) != 0)) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.impl.for"));
if (t_type_start(t_tok(t_toks, t_cur))) {
{
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
}
}
else {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text1("E1001.exptype", t_tok(t_toks, t_cur)));
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ni), (char*)(t_mk("None")));
}
}
}
}
else {
{
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_typ(t_toks, t_cur, t_lns)));
}
}
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("{")) != 0)) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.impl.body"));
}
}
else {
{
t_adv(t_cur);
}
}
ctron_list_push((ctron_list*)(t_ni), (char*)(t_p_items(t_toks, t_cur, t_lns, "impl")));
return t_ni;
}
int t_type_start(const char* t_t) 
{
if ((strlen((const char*)(t_t)) == 0)) {
{
return 0;
}
}
int32_t t_c = ctron_byte_at(t_t, 0);
if (t_or2(t_or2(((t_c >= 97) && (t_c <= 122)), ((t_c >= 65) && (t_c <= 90))), (t_c == 95))) {
{
return 1;
}
}
return t_or2(t_or3((strcmp((const char*)(t_t), (const char*)("(")) == 0), (strcmp((const char*)(t_t), (const char*)("&")) == 0), (strcmp((const char*)(t_t), (const char*)("fn")) == 0)), (strcmp((const char*)(t_t), (const char*)("[")) == 0));
}
ctron_list* t_p_trait(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list* t_nt = (ctron_list*)(t_mk("Trait"));
ctron_list_push((ctron_list*)(t_nt), t_nm);
ctron_list* t_tp = (ctron_list*)(t_mk("TPs"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("[")) == 0)) {
{
t_tp = (ctron_list*)(t_p_tpars(t_toks, t_cur));
}
}
ctron_list_push((ctron_list*)(t_nt), (char*)(t_tp));
ctron_list* t_sup = (ctron_list*)(t_mk("Bounds"));
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(":")) == 0)) {
{
t_adv(t_cur);
int t_more = 1;
while (t_more) {
ctron_list_push((ctron_list*)(t_sup), t_tok(t_toks, t_cur));
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("+")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_more = 0;
}
}
}
}
}
ctron_list_push((ctron_list*)(t_nt), (char*)(t_sup));
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nt), (char*)(t_p_items(t_toks, t_cur, t_lns, "trait")));
return t_nt;
}
ctron_list* t_p_enum(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
ctron_list* t_dr0 = (ctron_list*)(t_mk("Drvs"));
return t_p_enum2(t_toks, t_cur, t_lns, t_dr0, "");
}
int t_p_kw(const char* t_t) 
{
ctron_list* t_ks = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ks), "fn");
ctron_list_push((ctron_list*)(t_ks), "let");
ctron_list_push((ctron_list*)(t_ks), "var");
ctron_list_push((ctron_list*)(t_ks), "const");
ctron_list_push((ctron_list*)(t_ks), "static");
ctron_list_push((ctron_list*)(t_ks), "comptime");
ctron_list_push((ctron_list*)(t_ks), "if");
ctron_list_push((ctron_list*)(t_ks), "else");
ctron_list_push((ctron_list*)(t_ks), "match");
ctron_list_push((ctron_list*)(t_ks), "while");
ctron_list_push((ctron_list*)(t_ks), "for");
ctron_list_push((ctron_list*)(t_ks), "in");
ctron_list_push((ctron_list*)(t_ks), "break");
ctron_list_push((ctron_list*)(t_ks), "continue");
ctron_list_push((ctron_list*)(t_ks), "return");
ctron_list_push((ctron_list*)(t_ks), "struct");
ctron_list_push((ctron_list*)(t_ks), "class");
ctron_list_push((ctron_list*)(t_ks), "enum");
ctron_list_push((ctron_list*)(t_ks), "trait");
ctron_list_push((ctron_list*)(t_ks), "impl");
ctron_list_push((ctron_list*)(t_ks), "own");
ctron_list_push((ctron_list*)(t_ks), "scope");
ctron_list_push((ctron_list*)(t_ks), "test");
ctron_list_push((ctron_list*)(t_ks), "use");
ctron_list_push((ctron_list*)(t_ks), "pub");
ctron_list_push((ctron_list*)(t_ks), "extern");
ctron_list_push((ctron_list*)(t_ks), "prop");
ctron_list_push((ctron_list*)(t_ks), "true");
ctron_list_push((ctron_list*)(t_ks), "false");
ctron_list_push((ctron_list*)(t_ks), "void");
ctron_list_push((ctron_list*)(t_ks), "self");
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_ks))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ks))->items[t_k])), (const char*)(t_t)) == 0)) {
{
return 1;
}
}
t_k += 1;
}
return 0;
}
const char* t_p_use_alias(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
const char* t_t = t_tok(t_toks, t_cur);
if (t_or2((strcmp((const char*)(t_t), (const char*)("#EOF")) == 0), (strcmp((const char*)(t_t), (const char*)("NL")) == 0))) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.use.alias.ident"));
return "";
}
}
if ((!t_is_al(ctron_byte_at(t_t, 0)))) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.use.alias.ident"));
return "";
}
}
if (t_p_kw(t_t)) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.use.alias.ident"));
return "";
}
}
t_adv(t_cur);
return t_t;
}
ctron_list* t_p_use(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
t_adv(t_cur);
ctron_list* t_nu = (ctron_list*)(t_mk("Use"));
ctron_list* t_segs = (ctron_list*)(t_mk("Segs"));
ctron_list* t_syms = (ctron_list*)(t_mk("Syms"));
int t_braced = 0;
const char* t_ng_alias = "";
int t_more = 1;
{
const char* t_a = 0;
while (t_more) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_a = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_a), (const char*)(".")) == 0)) {
{
t_adv(t_cur);
}
}
else {
if ((strcmp((const char*)(t_a), (const char*)("{")) == 0)) {
{
t_adv(t_cur);
t_braced = 1;
int t_more2 = 1;
{
const char* t_b2 = 0;
while (t_more2) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_b2 = t_tok(t_toks, t_cur);
if (t_or2((strcmp((const char*)(t_b2), (const char*)("}")) == 0), (strcmp((const char*)(t_b2), (const char*)("#EOF")) == 0))) {
{
t_more2 = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_syms), t_b2);
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("as")) == 0)) {
{
t_adv(t_cur);
const char* t_al = t_p_use_alias(t_toks, t_cur, t_lns, t_cols, t_pdiags);
if ((strcmp((const char*)(t_al), (const char*)("")) == 0)) {
{
ctron_list_push((ctron_list*)(t_syms), t_b2);
}
}
else {
{
ctron_list_push((ctron_list*)(t_syms), t_al);
if (((((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) != 0) && (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) != 0)) && (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) != 0)) && (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) != 0))) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.use.alias.tail"));
t_adv(t_cur);
}
}
}
}
}
}
else {
{
ctron_list_push((ctron_list*)(t_syms), t_b2);
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("}")) == 0)) {
{
t_adv(t_cur);
}
}
t_more = 0;
}
}
else {
if ((strcmp((const char*)(t_a), (const char*)("#EOF")) == 0)) {
{
t_more = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_segs), t_a);
t_adv(t_cur);
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
const char* t_nx = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_nx), (const char*)("as")) == 0)) {
{
t_adv(t_cur);
t_ng_alias = t_p_use_alias(t_toks, t_cur, t_lns, t_cols, t_pdiags);
t_more = 0;
}
}
else {
if ((strcmp((const char*)(t_nx), (const char*)(".")) != 0)) {
{
t_more = 0;
}
}
}
}
}
}
}
}
}
if ((!t_braced)) {
{
const char* t_last = ((const char*)((ctron_list*)(t_segs))->items[(((ctron_list*)(t_segs))->n - 1)]);
ctron_list_push((ctron_list*)(t_syms), t_last);
if ((strcmp((const char*)(t_ng_alias), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_syms), t_ng_alias);
}
}
else {
{
ctron_list_push((ctron_list*)(t_syms), t_last);
}
}
}
}
ctron_list_push((ctron_list*)(t_nu), (char*)(t_segs));
ctron_list_push((ctron_list*)(t_nu), (char*)(t_syms));
return t_nu;
}
ctron_list* t_p_const(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
t_adv(t_cur);
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_ty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
t_adv(t_cur);
ctron_list* t_nc = (ctron_list*)(t_mk("Const"));
ctron_list_push((ctron_list*)(t_nc), t_nm);
ctron_list_push((ctron_list*)(t_nc), (char*)(t_ty));
ctron_list_push((ctron_list*)(t_nc), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
return t_nc;
}
ctron_list* t_p_static(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns) 
{
const char* t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
t_adv(t_cur);
const char* t_wv = "false";
const char* t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("var")) == 0)) {
{
t_wv = "true";
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("var")) == 0), (strcmp((const char*)(t_t), (const char*)("let")) == 0))) {
{
t_adv(t_cur);
}
}
const char* t_nm = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_ty = (ctron_list*)(t_p_typ(t_toks, t_cur, t_lns));
t_adv(t_cur);
ctron_list* t_ns = (ctron_list*)(t_mk("Static"));
ctron_list_push((ctron_list*)(t_ns), t_nm);
ctron_list_push((ctron_list*)(t_ns), t_wv);
ctron_list_push((ctron_list*)(t_ns), (char*)(t_ty));
ctron_list_push((ctron_list*)(t_ns), (char*)(t_p_stmt_expr(t_toks, t_cur, t_lns, "1")));
t_nstamp(t_ns, t_ln0);
return t_ns;
}
ctron_list* t_p_extern(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags, const char* t_anm) 
{
const char* t_ln0 = ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
t_adv(t_cur);
if ((ctron_byte_at(t_tok(t_toks, t_cur), 0) == 34)) {
{
const char* t_abi = t_tok(t_toks, t_cur);
t_adv(t_cur);
if ((!t_seq2(t_qtext(ctron_byte_slice(t_abi, 1, (strlen((const char*)(t_abi)) - 1))), "c"))) {
{
t_pdiag(t_pdiags, t_lns, t_cols, t_cur, "E1001", t_diag_text0("E1001.abi"));
}
}
}
}
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
ctron_list* t_fd = (ctron_list*)(t_p_fn2(t_toks, t_cur, t_lns, "FnExt"));
if ((strcmp((const char*)(t_anm), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_fd), t_anm);
}
}
t_nstamp(t_fd, t_ln0);
return t_fd;
}
ctron_list* t_p_file(ctron_list* t_toks, ctron_cell* t_cur, ctron_list* t_lns, ctron_list* t_cols, ctron_list* t_pdiags) 
{
ctron_list* t_nf = (ctron_list*)(t_mk("File"));
int t_go = 1;
{
int32_t t_p0 = 0;
const char* t_t = 0;
while (t_go) {
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
t_p0 = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("#EOF")) == 0)) {
{
t_go = 0;
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("#")) == 0)) {
{
const char* t_anm = "";
int t_moreattr = 1;
{
int t_skipnl = 0;
while (t_moreattr) {
t_skipnl = 1;
{
const char* t_nt = 0;
while (t_skipnl) {
t_nt = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_nt), (const char*)("NL")) == 0)) {
{
t_adv(t_cur);
}
}
else {
{
t_skipnl = 0;
}
}
}
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#")) != 0)) {
{
t_moreattr = 0;
}
}
else {
{
t_adv(t_cur);
t_adv(t_cur);
const char* t_g = t_tok(t_toks, t_cur);
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("(")) == 0)) {
{
t_g = ctron_str_concat((const char*)(t_g), (const char*)("("));
int32_t t_dep = 1;
t_adv(t_cur);
int t_morea = 1;
{
const char* t_at = 0;
while (t_morea) {
t_at = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_at), (const char*)("#EOF")) == 0)) {
{
t_morea = 0;
}
}
else {
if ((strcmp((const char*)(t_at), (const char*)("(")) == 0)) {
{
t_dep += 1;
t_g = ctron_str_concat((const char*)(t_g), (const char*)(t_at));
t_adv(t_cur);
}
}
else {
if ((strcmp((const char*)(t_at), (const char*)(")")) == 0)) {
{
t_dep -= 1;
t_adv(t_cur);
if ((t_dep == 0)) {
{
t_morea = 0;
}
}
else {
{
t_g = ctron_str_concat((const char*)(t_g), (const char*)(")"));
}
}
}
}
else {
{
t_g = ctron_str_concat((const char*)(t_g), (const char*)(t_at));
t_adv(t_cur);
}
}
}
}
}
}
t_g = ctron_str_concat((const char*)(t_g), (const char*)(")"));
}
}
while (((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) != 0) && (strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("#EOF")) != 0))) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("]")) == 0)) {
{
t_adv(t_cur);
}
}
if ((strcmp((const char*)(t_anm), (const char*)("")) != 0)) {
{
t_anm = ctron_str_concat((const char*)(t_anm), (const char*)(","));
}
}
t_anm = ctron_str_concat((const char*)(t_anm), (const char*)(t_g));
}
}
}
}
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("extern")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_extern(t_toks, t_cur, t_lns, t_cols, t_pdiags, t_anm)));
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("struct")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_struct2(t_toks, t_cur, t_lns, t_mk("Drvs"), t_anm)));
}
}
else {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("enum")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_enum2(t_toks, t_cur, t_lns, t_mk("Drvs"), t_anm)));
}
}
else {
{
ctron_list* t_fd = (ctron_list*)(t_p_fn2(t_toks, t_cur, t_lns, "Fn"));
ctron_list_push((ctron_list*)(t_fd), t_anm);
t_nstamp(t_fd, ((const char*)((ctron_list*)(t_lns))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
ctron_list_push((ctron_list*)(t_nf), (char*)(t_fd));
}
}
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("view")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_gui_block(t_toks, t_cur, "view")));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("style")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_gui_block(t_toks, t_cur, "style")));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("pub")) == 0)) {
{
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("(")) == 0)) {
{
t_adv(t_cur);
t_adv(t_cur);
t_adv(t_cur);
}
}
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
const char* t_t3 = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t3), (const char*)("fn")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_fn2(t_toks, t_cur, t_lns, "FnPub")));
}
}
else {
{
if ((strcmp((const char*)(t_t3), (const char*)("struct")) == 0)) {
{
ctron_list* t_ns3 = (ctron_list*)(t_p_struct2(t_toks, t_cur, t_lns, t_mk("Drvs"), ""));
ctron_list_push((ctron_list*)(t_ns3), "pub");
ctron_list_push((ctron_list*)(t_nf), (char*)(t_ns3));
}
}
else {
{
if ((strcmp((const char*)(t_t3), (const char*)("enum")) == 0)) {
{
ctron_list* t_ne3 = (ctron_list*)(t_p_enum2(t_toks, t_cur, t_lns, t_mk("Drvs"), ""));
ctron_list_push((ctron_list*)(t_ne3), "pub");
ctron_list_push((ctron_list*)(t_nf), (char*)(t_ne3));
}
}
else {
{
if ((strcmp((const char*)(t_t3), (const char*)("extern")) == 0)) {
{
ctron_list* t_nx3 = (ctron_list*)(t_p_extern(t_toks, t_cur, t_lns, t_cols, t_pdiags, ""));
ctron_list_push((ctron_list*)(t_nx3), "pub");
ctron_list_push((ctron_list*)(t_nf), (char*)(t_nx3));
}
}
else {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_fn(t_toks, t_cur, t_lns)));
}
}
}
}
}
}
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("comptime")) == 0)) {
{
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_fn2(t_toks, t_cur, t_lns, "FnC")));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("@")) == 0)) {
{
t_adv(t_cur);
t_adv(t_cur);
t_adv(t_cur);
ctron_list* t_dr2 = (ctron_list*)(t_mk("Drvs"));
int t_more3 = 1;
while (t_more3) {
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(")")) == 0)) {
{
t_more3 = 0;
}
}
else {
{
ctron_list_push((ctron_list*)(t_dr2), t_tok(t_toks, t_cur));
t_adv(t_cur);
if ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)(",")) == 0)) {
{
t_adv(t_cur);
}
}
}
}
}
t_adv(t_cur);
while ((strcmp((const char*)(t_tok(t_toks, t_cur)), (const char*)("NL")) == 0)) {
t_adv(t_cur);
}
const char* t_t2 = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t2), (const char*)("struct")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_struct2(t_toks, t_cur, t_lns, t_dr2, "")));
}
}
else {
if ((strcmp((const char*)(t_t2), (const char*)("enum")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_enum2(t_toks, t_cur, t_lns, t_dr2, "")));
}
}
else {
if ((strcmp((const char*)(t_t2), (const char*)("trait")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_trait(t_toks, t_cur, t_lns)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_fn(t_toks, t_cur, t_lns)));
}
}
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("test")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_test(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("extern")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_extern(t_toks, t_cur, t_lns, t_cols, t_pdiags, "")));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("enum")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_enum(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("struct")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_struct(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("class")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_class(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("impl")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_impl(t_toks, t_cur, t_lns, t_cols, t_pdiags)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("trait")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_trait(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("use")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_use(t_toks, t_cur, t_lns, t_cols, t_pdiags)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("const")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_const(t_toks, t_cur, t_lns)));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("static")) == 0)) {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_static(t_toks, t_cur, t_lns)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_nf), (char*)(t_p_fn(t_toks, t_cur, t_lns)));
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) == t_p0)) {
{
t_adv(t_cur);
}
}
}
}
return t_nf;
}
ctron_list* t_gui_block(ctron_list* t_toks, ctron_cell* t_cur, const char* t_kind) 
{
ctron_list* t_nd = (ctron_list*)(t_mk("GuiBlock"));
ctron_list_push((ctron_list*)(t_nd), t_kind);
t_adv(t_cur);
const char* t_name = t_tok(t_toks, t_cur);
t_adv(t_cur);
ctron_list_push((ctron_list*)(t_nd), t_name);
const char* t_props = "";
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_toks))->n)) {
{
const char* t_pt = t_tok(t_toks, t_cur);
if ((strlen((const char*)(t_pt)) == 1)) {
{
if ((ctron_byte_at(t_pt, 0) == 40)) {
{
t_adv(t_cur);
t_props = "( ";
int32_t t_pd = 1;
{
const char* t_t2 = 0;
while (((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_toks))->n) && (t_pd > 0))) {
t_t2 = t_tok(t_toks, t_cur);
t_adv(t_cur);
if ((strlen((const char*)(t_t2)) > 1)) {
{
if ((ctron_byte_at(t_t2, (strlen((const char*)(t_t2)) - 1)) == 126)) {
{
t_t2 = ctron_byte_slice(t_t2, 0, (strlen((const char*)(t_t2)) - 1));
}
}
}
}
if ((strlen((const char*)(t_t2)) == 1)) {
{
int32_t t_b2 = ctron_byte_at(t_t2, 0);
if ((t_b2 == 40)) {
{
t_pd += 1;
}
}
else {
{
if ((t_b2 == 41)) {
{
t_pd -= 1;
if ((t_pd == 0)) {
{
break;
}
}
}
}
}
}
}
}
t_props = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_props), (const char*)(t_t2))), (const char*)(" "));
}
}
t_props = ctron_str_concat((const char*)(t_props), (const char*)(")"));
}
}
}
}
}
}
const char* t_joined = t_name;
if ((strcmp((const char*)(t_kind), (const char*)("style")) == 0)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_toks))->n)) {
{
const char* t_pt2 = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_pt2), (const char*)("extends")) == 0)) {
{
t_adv(t_cur);
const char* t_pn2 = t_tok(t_toks, t_cur);
t_adv(t_cur);
t_joined = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_joined), (const char*)(" extends "))), (const char*)(t_pn2));
}
}
}
}
}
}
int32_t t_depth = 0;
int32_t t_toks_n = 1;
int t_started = 0;
{
const char* t_t = 0;
int t_one = 0;
int t_is_open = 0;
int t_is_close = 0;
while (1) {
t_t = t_tok(t_toks, t_cur);
if ((strcmp((const char*)(t_t), (const char*)("#EOF")) == 0)) {
{
ctron_panic(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("gui: 块未闭合 "), (const char*)(t_kind))), (const char*)(" "))), (const char*)(t_name)));
}
}
t_one = (strlen((const char*)(t_t)) == 1);
t_is_open = 0;
t_is_close = 0;
if (t_one) {
{
if ((ctron_byte_at(t_t, 0) == 123)) {
{
t_is_open = 1;
}
}
if ((ctron_byte_at(t_t, 0) == 125)) {
{
t_is_close = 1;
}
}
}
}
if (t_is_open) {
{
t_depth += 1;
t_started = 1;
if ((t_depth == 1)) {
{
t_adv(t_cur);
continue;
}
}
}
}
else {
{
if (t_is_close) {
{
t_depth -= 1;
if ((t_depth == 0)) {
{
t_adv(t_cur);
break;
}
}
}
}
}
}
if (t_started) {
{
t_joined = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_joined), (const char*)(" "))), (const char*)(t_t));
t_toks_n += 1;
}
}
t_adv(t_cur);
}
}
ctron_list_push((ctron_list*)(t_nd), ctron_i32_to_string((int32_t)(t_toks_n)));
ctron_list_push((ctron_list*)(t_nd), t_joined);
ctron_list_push((ctron_list*)(t_nd), t_props);
return t_nd;
}
int32_t t_gui_split(const char* t_joined, ctron_list* t_out) 
{
int32_t t_n = strlen((const char*)(t_joined));
int32_t t_i = 0;
{
int32_t t_start = 0;
while ((t_i < t_n)) {
t_start = t_i;
while ((t_i < t_n)) {
if ((ctron_byte_at(t_joined, t_i) == 32)) {
{
break;
}
}
t_i += 1;
}
if ((t_i > t_start)) {
{
ctron_list_push((ctron_list*)(t_out), ctron_byte_slice(t_joined, t_start, t_i));
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_hex1(int32_t t_b) 
{
if ((t_b >= 48)) {
{
if ((t_b <= 57)) {
{
return (t_b - 48);
}
}
}
}
if ((t_b >= 97)) {
{
if ((t_b <= 102)) {
{
return (t_b - 87);
}
}
}
}
if ((t_b >= 65)) {
{
if ((t_b <= 70)) {
{
return (t_b - 55);
}
}
}
}
ctron_panic("gui E8130: 颜色非法十六进制");
return 0;
}
const char* t_gui_unq(const char* t_v) 
{
if ((strlen((const char*)(t_v)) >= 2)) {
{
if ((ctron_byte_at(t_v, 0) == 34)) {
{
if ((ctron_byte_at(t_v, (strlen((const char*)(t_v)) - 1)) == 34)) {
{
return ctron_byte_slice(t_v, 1, (strlen((const char*)(t_v)) - 1));
}
}
}
}
}
}
return t_v;
}
const char* t_eat_tok(ctron_list* t_tks, ctron_cell* t_cur) 
{
const char* t_t = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
return t_t;
}
int32_t t_gui_parse_color(const char* t_v) 
{
if ((strlen((const char*)(t_v)) != 7)) {
{
ctron_panic(ctron_str_concat((const char*)("gui E8130: 颜色须 #RRGGBB,得 "), (const char*)(t_v)));
}
}
if ((ctron_byte_at(t_v, 0) != 35)) {
{
ctron_panic(ctron_str_concat((const char*)("gui E8130: 颜色须 # 开头,得 "), (const char*)(t_v)));
}
}
int32_t t_r = ((t_gui_hex1(ctron_byte_at(t_v, 1)) * 16) + t_gui_hex1(ctron_byte_at(t_v, 2)));
int32_t t_g = ((t_gui_hex1(ctron_byte_at(t_v, 3)) * 16) + t_gui_hex1(ctron_byte_at(t_v, 4)));
int32_t t_b = ((t_gui_hex1(ctron_byte_at(t_v, 5)) * 16) + t_gui_hex1(ctron_byte_at(t_v, 6)));
return (((t_r * 65536) + (t_g * 256)) + t_b);
}
int32_t t_gui_dump_file(ctron_list* t_file) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
ctron_list* t_tks = (ctron_list*)(ctron_list_new());
t_gui_split(((const char*)((ctron_list*)(t_d))->items[4]), t_tks);
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
ctron_list* t_out = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
t_gui_lower_view(t_tks, t_cur, "  ", t_out);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("style")) == 0)) {
{
t_gui_lower_style(t_tks, t_cur, "  ", t_out);
}
}
}
}
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_lower_element(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out) 
{
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("<")) != 0)) {
{
ctron_panic("gui E8100: 期待 <");
}
}
const char* t_tag = t_eat_tok(t_tks, t_cur);
int32_t t_fb5 = ctron_byte_at(t_tag, 0);
if ((t_fb5 >= 65)) {
{
if ((t_fb5 <= 90)) {
{
int t_saws5 = 0;
{
const char* t_pk5 = 0;
const char* t_vw5 = 0;
while ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)(">")) != 0)) {
t_pk5 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk5)) == 1)) {
{
if ((ctron_byte_at(t_pk5, 0) == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_saws5 = 1;
break;
}
}
}
}
t_eat_tok(t_tks, t_cur);
t_eat_tok(t_tks, t_cur);
t_vw5 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_vw5)) == 1)) {
{
if ((ctron_byte_at(t_vw5, 0) == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_gui_bx_expr(t_tks, t_cur);
}
}
else {
{
t_eat_tok(t_tks, t_cur);
}
}
}
}
else {
{
t_eat_tok(t_tks, t_cur);
}
}
}
}
t_eat_tok(t_tks, t_cur);
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("el call:"))), (const char*)(t_tag))));
if ((!t_saws5)) {
{
while (1) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("<")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)])), (const char*)("/")) == 0)) {
{
break;
}
}
t_gui_lower_element(t_tks, t_cur, ctron_str_concat((const char*)(t_ind), (const char*)("  ")), t_out);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("#EOF")) == 0)) {
{
ctron_panic("gui E8100: 未终止");
}
}
t_eat_tok(t_tks, t_cur);
}
}
}
t_eat_tok(t_tks, t_cur);
t_eat_tok(t_tks, t_cur);
const char* t_et5 = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_et5), (const char*)(t_tag)) != 0)) {
{
ctron_panic(ctron_str_concat((const char*)("gui E8100: 闭合标签不匹配 "), (const char*)(t_et5)));
}
}
t_eat_tok(t_tks, t_cur);
}
}
return 0;
}
}
}
}
const char* t_c = "";
const char* t_onn = "";
const char* t_onf = "";
const char* t_bv = "";
const char* t_itemvar = "";
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
t_itemvar = t_eat_tok(t_tks, t_cur);
}
}
int t_selfc = 0;
{
const char* t_pk = 0;
const char* t_an = 0;
while ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)(">")) != 0)) {
t_pk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk)) == 1)) {
{
if ((ctron_byte_at(t_pk, 0) == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_selfc = 1;
break;
}
}
}
}
t_an = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_an), (const char*)("on")) == 0)) {
{
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)(":")) != 0)) {
{
ctron_panic("gui E8100: on 期待冒号");
}
}
t_onn = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("=")) != 0)) {
{
ctron_panic("gui E8100: on 期待等号");
}
}
const char* t_ob = t_eat_tok(t_tks, t_cur);
if ((strlen((const char*)(t_ob)) != 1)) {
{
ctron_panic("gui E8100: on 期待块开");
}
}
if ((ctron_byte_at(t_ob, 0) != 123)) {
{
ctron_panic("gui E8100: on 期待块开");
}
}
t_onf = t_gui_bx_expr(t_tks, t_cur);
}
}
else {
{
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("=")) != 0)) {
{
ctron_panic("gui E8100: 属性期待等号");
}
}
const char* t_av = "";
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]))) == 1)) {
{
if ((ctron_byte_at(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]), 0) == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_av = ctron_str_concat((const char*)(ctron_str_concat((const char*)("{"), (const char*)(t_gui_bx_expr(t_tks, t_cur)))), (const char*)("}"));
}
}
}
}
if ((strcmp((const char*)(t_av), (const char*)("")) == 0)) {
{
t_av = t_gui_unq(t_eat_tok(t_tks, t_cur));
}
}
if ((strcmp((const char*)(t_an), (const char*)("class")) == 0)) {
{
t_c = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("cond")) == 0)) {
{
t_bv = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("bind")) == 0)) {
{
t_bv = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("in")) == 0)) {
{
t_bv = t_av;
}
}
}
}
}
}
t_eat_tok(t_tks, t_cur);
const char* t_line = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("el "))), (const char*)(t_tag));
if ((strcmp((const char*)(t_itemvar), (const char*)("")) != 0)) {
{
t_line = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_line), (const char*)(" item="))), (const char*)(t_itemvar));
}
}
if ((strcmp((const char*)(t_c), (const char*)("")) != 0)) {
{
t_line = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_line), (const char*)(" cls="))), (const char*)(t_c));
}
}
if ((strcmp((const char*)(t_bv), (const char*)("")) != 0)) {
{
t_line = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_line), (const char*)(" bind="))), (const char*)(t_bv));
}
}
if ((strcmp((const char*)(t_onn), (const char*)("")) != 0)) {
{
t_line = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_line), (const char*)(" on:"))), (const char*)(t_onn))), (const char*)("="))), (const char*)(t_onf));
}
}
printf("%s\n", (const char*)(t_line));
ctron_list_push((ctron_list*)(t_out), t_line);
if (t_selfc) {
{
return 0;
}
}
const char* t_lit = "";
while (1) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("<")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)])), (const char*)("/")) == 0)) {
{
break;
}
}
if ((strcmp((const char*)(t_lit), (const char*)("")) != 0)) {
{
const char* t_tl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("  "))), (const char*)(t_tag))), (const char*)(" text=\""))), (const char*)(t_lit))), (const char*)("\""));
printf("%s\n", (const char*)(t_tl));
ctron_list_push((ctron_list*)(t_out), t_tl);
t_lit = "";
}
}
t_gui_lower_element(t_tks, t_cur, ctron_str_concat((const char*)(t_ind), (const char*)("  ")), t_out);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("#EOF")) == 0)) {
{
ctron_panic("gui E8100: 未终止");
}
}
const char* t_w = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_lit), (const char*)("")) != 0)) {
{
t_lit = ctron_str_concat((const char*)(t_lit), (const char*)(" "));
}
}
t_lit = ctron_str_concat((const char*)(t_lit), (const char*)(t_w));
}
}
}
if ((strcmp((const char*)(t_lit), (const char*)("")) != 0)) {
{
const char* t_tline = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("  "))), (const char*)(t_tag))), (const char*)(" text=\""))), (const char*)(t_lit))), (const char*)("\""));
printf("%s\n", (const char*)(t_tline));
ctron_list_push((ctron_list*)(t_out), t_tline);
}
}
t_eat_tok(t_tks, t_cur);
t_eat_tok(t_tks, t_cur);
const char* t_et = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_et), (const char*)(t_tag)) != 0)) {
{
ctron_panic(ctron_str_concat((const char*)("gui E8100: 闭合标签不匹配 "), (const char*)(t_et)));
}
}
t_eat_tok(t_tks, t_cur);
return 0;
}
int32_t t_gui_lower_view(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out) 
{
const char* t_name = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("gui view "))), (const char*)(t_name))));
ctron_list_push((ctron_list*)(t_out), ctron_str_concat((const char*)("view "), (const char*)(t_name)));
t_gui_lower_element(t_tks, t_cur, ctron_str_concat((const char*)(t_ind), (const char*)("  ")), t_out);
return 0;
}
int32_t t_gui_lower_style(ctron_list* t_tks, ctron_cell* t_cur, const char* t_ind, ctron_list* t_out) 
{
const char* t_name = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("extends")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("gui style "))), (const char*)(t_name))));
ctron_list_push((ctron_list*)(t_out), ctron_str_concat((const char*)("style "), (const char*)(t_name)));
{
const char* t_p = 0;
const char* t_v = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_p = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)(":")) != 0)) {
{
ctron_panic(ctron_str_concat((const char*)("gui E8130: 期待 :,得 "), (const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]))));
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_v = t_gui_unq(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]));
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
if ((strcmp((const char*)(t_p), (const char*)("bg")) == 0)) {
{
int32_t t_packed = t_gui_parse_color(t_v);
if ((t_packed < 0)) {
{
ctron_panic("gui E8130: 颜色解析失败");
}
}
}
}
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_ind), (const char*)("  "))), (const char*)(t_p))), (const char*)("="))), (const char*)(t_v))));
ctron_list_push((ctron_list*)(t_out), ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_p), (const char*)("="))), (const char*)(t_v)));
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)(";")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
return 0;
}
const char* t_gui_blocks_src(ctron_list* t_file) 
{
const char* t_out = "";
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((ctron_len((const void*)(t_d)) < 3)) {
{
t_i += 1;
continue;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_d))->items[1])))), (const char*)(" "))), (const char*)(((const char*)((ctron_list*)(t_d))->items[2])))), (const char*)(((const char*)((ctron_list*)(t_d))->items[5])))), (const char*)(" {"));
ctron_list* t_tks = (ctron_list*)(ctron_list_new());
t_gui_split(((const char*)((ctron_list*)(t_d))->items[4]), t_tks);
int32_t t_k = 1;
{
const char* t_tk = 0;
while ((t_k < ((ctron_list*)(t_tks))->n)) {
t_tk = ((const char*)((ctron_list*)(t_tks))->items[t_k]);
if ((strlen((const char*)(t_tk)) > 1)) {
{
if ((ctron_byte_at(t_tk, (strlen((const char*)(t_tk)) - 1)) == 126)) {
{
t_tk = ctron_byte_slice(t_tk, 0, (strlen((const char*)(t_tk)) - 1));
}
}
}
}
if ((strcmp((const char*)(t_tk), (const char*)("<")) == 0)) {
{
if (((t_k + 1) < ((ctron_list*)(t_tks))->n)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[(t_k + 1)])), (const char*)("/")) == 0)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" </"));
t_k += 2;
continue;
}
}
}
}
}
}
if ((strcmp((const char*)(t_tk), (const char*)("=")) == 0)) {
{
if (((t_k + 1) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_nk2 = ((const char*)((ctron_list*)(t_tks))->items[(t_k + 1)]);
if ((strlen((const char*)(t_nk2)) == 1)) {
{
if ((ctron_byte_at(t_nk2, 0) == 123)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" ={"));
t_k += 2;
continue;
}
}
}
}
}
}
}
}
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(" "))), (const char*)(t_tk));
t_k += 1;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" } "));
}
}
t_i += 1;
}
}
return t_out;
}
int32_t t_gui_field_tables(ctron_list* t_file, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv) 
{
int32_t t_i = 1;
{
const char* t_ds = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_ds = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[0])), (const char*)("Struct")) == 0)) {
{
if ((ctron_len((const void*)(t_ds)) > 4)) {
{
const char* t_fsx = ((const char*)((ctron_list*)(t_ds))->items[4]);
int32_t t_fi = 1;
{
const char* t_fd = 0;
while ((t_fi < ctron_len((const void*)(t_fsx)))) {
t_fd = ((const char*)((ctron_list*)(t_fsx))->items[t_fi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_fd))->items[0])), (const char*)("Field")) == 0)) {
{
ctron_list_push((ctron_list*)(t_stns), ((const char*)((ctron_list*)(t_ds))->items[1]));
ctron_list_push((ctron_list*)(t_sfk), ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_ds))->items[1])), (const char*)("."))), (const char*)(((const char*)((ctron_list*)(t_fd))->items[2]))));
ctron_list_push((ctron_list*)(t_stv), t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_fd))->items[3]))));
}
}
t_fi += 1;
}
}
}
}
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_ds_views(ctron_list* t_file, ctron_list* t_vnames, ctron_list* t_vprops) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
ctron_list_push((ctron_list*)(t_vnames), ((const char*)((ctron_list*)(t_d))->items[2]));
if ((ctron_len((const void*)(t_d)) > 5)) {
{
ctron_list_push((ctron_list*)(t_vprops), ((const char*)((ctron_list*)(t_d))->items[5]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_vprops), "");
}
}
}
}
}
}
t_i += 1;
}
}
return 0;
}
const char* t_gui_ds_find_expr(ctron_list* t_e, ctron_list* t_views, ctron_list* t_names) 
{
if ((((ctron_list*)(t_e))->n < 1)) {
{
return "";
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1])), (const char*)("run")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 2)) {
{
const char* t_a = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a))->items[0])), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))) > 1)) {
{
if (t_gui_ck_has_style(t_views, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1]))) {
{
ctron_list_push((ctron_list*)(t_names), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1]));
int32_t t_k2 = 1;
while ((t_k2 < ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))))) {
ctron_list_push((ctron_list*)(t_names), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])))->items[t_k2])))->items[1]));
t_k2 += 1;
}
return ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1]);
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
const char* t_r = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_views, t_names);
if ((strcmp((const char*)(t_r), (const char*)("")) != 0)) {
{
return t_r;
}
}
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))))) {
t_r = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[t_i])), t_views, t_names);
if ((strcmp((const char*)(t_r), (const char*)("")) != 0)) {
{
return t_r;
}
}
t_i += 1;
}
return "";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_r2 = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_views, t_names);
if ((strcmp((const char*)(t_r2), (const char*)("")) != 0)) {
{
return t_r2;
}
}
return t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_views, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_views, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
return t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_views, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_views, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
return t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_views, t_names);
}
}
return "";
}
const char* t_gui_ds_find_block(ctron_list* t_b, ctron_list* t_views, ctron_list* t_names) 
{
int32_t t_i = 1;
{
const char* t_s = 0;
const char* t_t = 0;
const char* t_hit = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_s = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_s))->items[0]);
t_hit = "";
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(t_s)) > 1)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_views, t_names);
}
}
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_views, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_views, t_names);
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_views, t_names);
}
}
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_views, t_names);
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_views, t_names);
}
}
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_views, t_names);
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_views, t_names);
}
}
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])))->items[0])), (const char*)("If")) == 0)) {
{
t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_views, t_names);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])))->items[1])), t_views, t_names);
}
}
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_views, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_views, t_names);
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_views, t_names);
}
}
}
}
else {
{
t_hit = t_gui_ds_find_expr((ctron_list*)(t_s), t_views, t_names);
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(t_hit), (const char*)("")) != 0)) {
{
return t_hit;
}
}
t_i += 1;
}
}
return "";
}
const char* t_gui_ds_find_file(ctron_list* t_file, ctron_list* t_views, ctron_list* t_names) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0)) {
{
const char* t_hit = t_gui_ds_find_block((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_views, t_names);
if ((strcmp((const char*)(t_hit), (const char*)("")) != 0)) {
{
return t_hit;
}
}
}
}
t_i += 1;
}
}
return "";
}
const char* t_gui_ds_path_ty(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv) 
{
ctron_list* t_segs = (ctron_list*)(ctron_list_new());
int32_t t_i = 0;
int32_t t_start = 0;
while ((t_i < strlen((const char*)(t_path)))) {
if ((ctron_byte_at(t_path, t_i) == 46)) {
{
ctron_list_push((ctron_list*)(t_segs), ctron_byte_slice(t_path, t_start, t_i));
t_start = (t_i + 1);
}
}
t_i += 1;
}
ctron_list_push((ctron_list*)(t_segs), ctron_byte_slice(t_path, t_start, strlen((const char*)(t_path))));
const char* t_root = ((const char*)((ctron_list*)(t_segs))->items[0]);
if ((!t_gui_ck_has_style(t_pnames, t_root))) {
{
return "";
}
}
const char* t_cur = "";
int32_t t_ri = 0;
while ((t_ri < ((ctron_list*)(t_pnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_ri])), (const char*)(t_root)) == 0)) {
{
t_cur = t_gui_ty_head_txt(((const char*)((ctron_list*)(t_ptyps))->items[t_ri]));
t_ri = ((ctron_list*)(t_pnames))->n;
}
}
t_ri += 1;
}
int32_t t_si = 1;
{
const char* t_seg = 0;
int t_last = 0;
const char* t_fkey = 0;
while ((t_si < ((ctron_list*)(t_segs))->n)) {
t_seg = ((const char*)((ctron_list*)(t_segs))->items[t_si]);
t_last = (t_si == (((ctron_list*)(t_segs))->n - 1));
t_fkey = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_cur), (const char*)("."))), (const char*)(t_seg));
if (t_gui_ck_has_style(t_sfk, t_fkey)) {
{
int32_t t_fi = 0;
while ((t_fi < ((ctron_list*)(t_sfk))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sfk))->items[t_fi])), (const char*)(t_fkey)) == 0)) {
{
t_cur = t_gui_ty_head_txt(((const char*)((ctron_list*)(t_stv))->items[t_fi]));
t_fi = ((ctron_list*)(t_sfk))->n;
}
}
t_fi += 1;
}
}
}
else {
{
if ((strcmp((const char*)(t_seg), (const char*)("len")) == 0)) {
{
if (t_last) {
{
return "I32";
}
}
return "";
}
}
return "";
}
}
t_si += 1;
}
}
return t_cur;
}
const char* t_gui_ds_rw(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptoks) 
{
const char* t_root = t_path;
int32_t t_di = 0;
while ((t_di < strlen((const char*)(t_path)))) {
if ((ctron_byte_at(t_path, t_di) == 46)) {
{
t_root = ctron_byte_slice(t_path, 0, t_di);
t_di = strlen((const char*)(t_path));
}
}
t_di += 1;
}
int32_t t_ri = 0;
while ((t_ri < ((ctron_list*)(t_pnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_ri])), (const char*)(t_root)) == 0)) {
{
return ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_ptoks))->items[t_ri])), (const char*)(ctron_byte_slice(t_path, strlen((const char*)(t_root)), strlen((const char*)(t_path)))));
}
}
t_ri += 1;
}
return t_path;
}
int t_gui_is_ctml(const char* t_p) 
{
if ((strlen((const char*)(t_p)) < 5)) {
{
return 0;
}
}
int32_t t_i = 0;
while ((t_i < 5)) {
if ((ctron_byte_at(t_p, ((strlen((const char*)(t_p)) - 5) + t_i)) != ctron_byte_at(".ctml", t_i))) {
{
return 0;
}
}
t_i += 1;
}
return 1;
}
int32_t t_gui_ctml_tokens(const char* t_src, ctron_list* t_out) 
{
int32_t t_n = strlen((const char*)(t_src));
int32_t t_i = 0;
{
int32_t t_b = 0;
while ((t_i < t_n)) {
t_b = ctron_byte_at(t_src, t_i);
if (((((t_b == 32) || (t_b == 9)) || (t_b == 10)) || (t_b == 13))) {
{
t_i += 1;
}
}
else {
{
if ((t_b == 47)) {
{
if (((t_i + 1) < t_n)) {
{
if ((ctron_byte_at(t_src, (t_i + 1)) == 47)) {
{
while ((t_i < t_n)) {
if ((ctron_byte_at(t_src, t_i) == 10)) {
{
break;
}
}
t_i += 1;
}
continue;
}
}
}
}
ctron_list_push((ctron_list*)(t_out), "/");
t_i += 1;
}
}
else {
{
if ((t_b == 34)) {
{
int32_t t_j = (t_i + 1);
{
int32_t t_cb = 0;
while ((t_j < t_n)) {
t_cb = ctron_byte_at(t_src, t_j);
if ((t_cb == 92)) {
{
t_j += 2;
continue;
}
}
if ((t_cb == 34)) {
{
break;
}
}
t_j += 1;
}
}
if ((t_j >= t_n)) {
{
t_j = (t_n - 1);
}
}
ctron_list_push((ctron_list*)(t_out), ctron_byte_slice(t_src, t_i, (t_j + 1)));
t_i = (t_j + 1);
}
}
else {
{
if ((((((((t_b == 60) || (t_b == 62)) || (t_b == 61)) || (t_b == 123)) || (t_b == 125)) || (t_b == 58)) || (t_b == 59))) {
{
ctron_list_push((ctron_list*)(t_out), ctron_byte_slice(t_src, t_i, (t_i + 1)));
t_i += 1;
}
}
else {
{
int32_t t_j2 = t_i;
{
int32_t t_wb = 0;
while ((t_j2 < t_n)) {
t_wb = ctron_byte_at(t_src, t_j2);
if (((((t_wb == 32) || (t_wb == 9)) || (t_wb == 10)) || (t_wb == 13))) {
{
break;
}
}
if ((((((((((t_wb == 60) || (t_wb == 62)) || (t_wb == 61)) || (t_wb == 123)) || (t_wb == 125)) || (t_wb == 58)) || (t_wb == 59)) || (t_wb == 47)) || (t_wb == 34))) {
{
break;
}
}
t_j2 += 1;
}
}
if ((t_j2 > t_i)) {
{
ctron_list_push((ctron_list*)(t_out), ctron_byte_slice(t_src, t_i, t_j2));
t_i = t_j2;
}
}
else {
{
t_i += 1;
}
}
}
}
}
}
}
}
}
}
}
}
return 0;
}
ctron_list* t_gui_ctml_file(const char* t_src) 
{
ctron_list* t_toks = (ctron_list*)(ctron_list_new());
t_gui_ctml_tokens(t_src, t_toks);
ctron_list* t_file = (ctron_list*)(t_mk("File"));
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
{
const char* t_t = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_toks))->n)) {
t_t = ((const char*)((ctron_list*)(t_toks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_t), (const char*)("view")) == 0)) {
{
ctron_list_push((ctron_list*)(t_file), (char*)(t_gui_block(t_toks, t_cur, "view")));
}
}
else {
{
if ((strcmp((const char*)(t_t), (const char*)("style")) == 0)) {
{
ctron_list_push((ctron_list*)(t_file), (char*)(t_gui_block(t_toks, t_cur, "style")));
}
}
else {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
}
}
return t_file;
}
int t_gui_ck_has_style(ctron_list* t_sn, const char* t_name) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_sn))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sn))->items[t_i])), (const char*)(t_name)) == 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
void t_gui_bx_roots(const char* t_e, ctron_list* t_out) 
{
int32_t t_i = 0;
int t_instr = 0;
{
int32_t t_b = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_b = ctron_byte_at(t_e, t_i);
if (t_instr) {
{
if ((t_b == 92)) {
{
t_i += 1;
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 0;
}
}
}
}
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 1;
}
}
else {
{
if (((((t_b >= 65) && (t_b <= 90)) || ((t_b >= 97) && (t_b <= 122))) || (t_b == 95))) {
{
int32_t t_start = t_i;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_c = ctron_byte_at(t_e, t_i);
if ((t_c == 46)) {
{
break;
}
}
if ((((((t_c >= 65) && (t_c <= 90)) || ((t_c >= 97) && (t_c <= 122))) || ((t_c >= 48) && (t_c <= 57))) || (t_c == 95))) {
{
t_i += 1;
}
}
else {
{
break;
}
}
}
}
const char* t_root = ctron_byte_slice(t_e, t_start, t_i);
{
int32_t t_j = 0;
int32_t t_c2 = 0;
while ((t_i < strlen((const char*)(t_e)))) {
if ((ctron_byte_at(t_e, t_i) != 46)) {
{
break;
}
}
t_j = (t_i + 1);
if ((t_j >= strlen((const char*)(t_e)))) {
{
break;
}
}
t_c2 = ctron_byte_at(t_e, t_j);
if ((!((((t_c2 >= 65) && (t_c2 <= 90)) || ((t_c2 >= 97) && (t_c2 <= 122))) || (t_c2 == 95)))) {
{
break;
}
}
t_i = t_j;
{
int32_t t_c3 = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_c3 = ctron_byte_at(t_e, t_i);
if ((t_c3 == 46)) {
{
break;
}
}
if ((((((t_c3 >= 65) && (t_c3 <= 90)) || ((t_c3 >= 97) && (t_c3 <= 122))) || ((t_c3 >= 48) && (t_c3 <= 57))) || (t_c3 == 95))) {
{
t_i += 1;
}
}
else {
{
break;
}
}
}
}
}
}
if ((!t_gui_ck_has_style(t_out, t_root))) {
{
ctron_list_push((ctron_list*)(t_out), t_root);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
int32_t t_gui_fn_arity(ctron_list* t_d) 
{
if ((((ctron_list*)(t_d))->n <= 3)) {
{
return (-2);
}
}
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ps))->items[0])), (const char*)("Ps")) != 0)) {
{
return (-2);
}
}
return (ctron_len((const void*)(t_ps)) - 1);
}
int t_gui_ty_is_box(ctron_list* t_t) 
{
if ((((ctron_list*)(t_t))->n > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_t))->items[0])), (const char*)("Named")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_t))->items[1])), (const char*)("Box")) == 0)) {
{
return 1;
}
}
}
}
}
}
return 0;
}
ctron_list* t_gui_fn_ptyps(ctron_list* t_file, const char* t_nm) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnPub")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnExt")) == 0)))) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)(t_nm)) == 0))) {
{
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_k = 1;
{
const char* t_pr = 0;
while ((t_k < ctron_len((const void*)(t_ps)))) {
t_pr = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pr))->items[0])), (const char*)("Param")) == 0)) {
{
const char* t_th = t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_pr))->items[3])));
if (t_gui_ty_is_box((ctron_list*)(((const char*)((ctron_list*)(t_pr))->items[3])))) {
{
t_th = ctron_str_concat((const char*)("Box:"), (const char*)(t_th));
}
}
ctron_list_push((ctron_list*)(t_out), t_th);
}
}
t_k += 1;
}
}
return t_out;
}
}
t_i += 1;
}
}
return t_out;
}
int32_t t_gui_ck_fn_argc(ctron_list* t_fns, ctron_list* t_fna, const char* t_name) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_fns))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_fns))->items[t_i])), (const char*)(t_name)) == 0)) {
{
return ((int32_t)(long)((ctron_list*)(t_fna))->items[t_i]);
}
}
t_i += 1;
}
return (-1);
}
const char* t_gui_ev_head2(const char* t_e, ctron_list* t_argc, ctron_list* t_aout) 
{
int32_t t_i = 0;
{
int32_t t_b = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_b = ctron_byte_at(t_e, t_i);
if (((((t_b == 95) || ((t_b >= 65) && (t_b <= 90))) || ((t_b >= 97) && (t_b <= 122))) || ((t_b >= 48) && (t_b <= 57)))) {
{
t_i += 1;
}
}
else {
{
break;
}
}
}
}
const char* t_head = ctron_byte_slice(t_e, 0, t_i);
if ((t_i == strlen((const char*)(t_e)))) {
{
if ((strlen((const char*)(t_head)) == 0)) {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((-2)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((-1)));
}
}
return t_head;
}
}
if ((strlen((const char*)(t_head)) == 0)) {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((-2)));
return t_head;
}
}
if ((ctron_byte_at(t_e, t_i) != 40)) {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((-2)));
return t_head;
}
}
int32_t t_j = (t_i + 1);
int32_t t_depth = 1;
int t_instr = 0;
int32_t t_commas = 0;
int t_seen = 0;
int32_t t_close = (-1);
{
int32_t t_c = 0;
while ((t_j < strlen((const char*)(t_e)))) {
t_c = ctron_byte_at(t_e, t_j);
if (t_instr) {
{
if ((t_c == 92)) {
{
t_j += 1;
}
}
else {
{
if ((t_c == 34)) {
{
t_instr = 0;
}
}
}
}
}
}
else {
{
if ((t_c == 34)) {
{
t_instr = 1;
}
}
else {
{
if ((t_c == 40)) {
{
t_depth += 1;
}
}
else {
{
if ((t_c == 41)) {
{
t_depth -= 1;
if ((t_depth == 0)) {
{
t_close = t_j;
break;
}
}
}
}
else {
{
if ((t_c == 44)) {
{
if ((t_depth == 1)) {
{
t_commas += 1;
}
}
}
}
}
}
}
}
}
}
if (((((t_c != 32) && (t_c != 9)) && (t_c != 10)) && (t_c != 13))) {
{
t_seen = 1;
}
}
}
}
t_j += 1;
}
}
if ((t_close != (strlen((const char*)(t_e)) - 1))) {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((-2)));
return t_head;
}
}
if (t_seen) {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)((t_commas + 1)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_argc), (const char*)(long)(0));
}
}
ctron_list_push((ctron_list*)(t_aout), ctron_byte_slice(t_e, (t_i + 1), t_close));
return t_head;
}
void t_gui_bx_dotted(const char* t_e, ctron_list* t_out) 
{
int32_t t_i = 0;
int t_instr = 0;
{
int32_t t_b = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_b = ctron_byte_at(t_e, t_i);
if (t_instr) {
{
if ((t_b == 92)) {
{
t_i += 1;
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 0;
}
}
}
}
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 1;
}
}
else {
{
if (((((t_b >= 65) && (t_b <= 90)) || ((t_b >= 97) && (t_b <= 122))) || (t_b == 95))) {
{
int32_t t_start = t_i;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_c = ctron_byte_at(t_e, t_i);
if ((((((t_c == 46) || (t_c == 95)) || ((t_c >= 65) && (t_c <= 90))) || ((t_c >= 97) && (t_c <= 122))) || ((t_c >= 48) && (t_c <= 57)))) {
{
t_i += 1;
}
}
else {
{
break;
}
}
}
}
const char* t_path = ctron_byte_slice(t_e, t_start, t_i);
if ((!t_gui_ck_has_style(t_out, t_path))) {
{
ctron_list_push((ctron_list*)(t_out), t_path);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
int32_t t_gui_props_typed(const char* t_d5, ctron_list* t_names, ctron_list* t_types) 
{
if ((strlen((const char*)(t_d5)) > 2)) {
{
const char* t_seg = ctron_byte_slice(t_d5, 2, (strlen((const char*)(t_d5)) - 2));
int32_t t_pi = 0;
{
int32_t t_ps = 0;
const char* t_unit = 0;
while ((t_pi < strlen((const char*)(t_seg)))) {
t_ps = t_pi;
while ((t_pi < strlen((const char*)(t_seg)))) {
if ((ctron_byte_at(t_seg, t_pi) == 44)) {
{
break;
}
}
t_pi += 1;
}
t_unit = ctron_byte_slice(t_seg, t_ps, t_pi);
int32_t t_ui = 0;
while ((t_ui < strlen((const char*)(t_unit)))) {
if ((ctron_byte_at(t_unit, t_ui) == 58)) {
{
const char* t_pn = ctron_byte_slice(t_unit, 0, t_ui);
const char* t_pt = ctron_byte_slice(t_unit, (t_ui + 1), strlen((const char*)(t_unit)));
ctron_list_push((ctron_list*)(t_names), t_gui_trim(t_pn));
ctron_list_push((ctron_list*)(t_types), t_gui_trim(t_pt));
t_ui = strlen((const char*)(t_unit));
}
}
t_ui += 1;
}
t_pi += 1;
}
}
}
}
return 0;
}
const char* t_gui_trim(const char* t_s) 
{
int32_t t_a = 0;
int32_t t_b = strlen((const char*)(t_s));
{
int32_t t_c = 0;
while ((t_a < t_b)) {
t_c = ctron_byte_at(t_s, t_a);
if (((t_c == 32) || (t_c == 9))) {
{
t_a += 1;
}
}
else {
{
break;
}
}
}
}
{
int32_t t_c2 = 0;
while ((t_b > t_a)) {
t_c2 = ctron_byte_at(t_s, (t_b - 1));
if (((t_c2 == 32) || (t_c2 == 9))) {
{
t_b -= 1;
}
}
else {
{
break;
}
}
}
}
return ctron_byte_slice(t_s, t_a, t_b);
}
const char* t_gui_ty_head_txt(const char* t_t) 
{
const char* t_head = "";
int32_t t_i = 0;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_t)))) {
t_c = ctron_byte_at(t_t, t_i);
if (((((t_c == 95) || ((t_c >= 65) && (t_c <= 90))) || ((t_c >= 97) && (t_c <= 122))) || ((t_c >= 48) && (t_c <= 57)))) {
{
t_head = ctron_str_concat((const char*)(t_head), (const char*)(ctron_byte_slice(t_t, t_i, (t_i + 1))));
t_i += 1;
}
}
else {
{
break;
}
}
}
}
if ((strcmp((const char*)(t_head), (const char*)("Box")) == 0)) {
{
int32_t t_lo = (-1);
int32_t t_k = 0;
while ((t_k < strlen((const char*)(t_t)))) {
if ((ctron_byte_at(t_t, t_k) == 91)) {
{
t_lo = t_k;
break;
}
}
t_k += 1;
}
if ((t_lo >= 0)) {
{
return t_gui_ty_head_txt(t_gui_trim(ctron_byte_slice(t_t, (t_lo + 1), strlen((const char*)(t_t)))));
}
}
}
}
return t_head;
}
int32_t t_gui_ck_prop_path(const char* t_path, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_gdiags, const char* t_where) 
{
ctron_list* t_segs = (ctron_list*)(ctron_list_new());
int32_t t_i = 0;
int32_t t_start = 0;
while ((t_i < strlen((const char*)(t_path)))) {
if ((ctron_byte_at(t_path, t_i) == 46)) {
{
ctron_list_push((ctron_list*)(t_segs), ctron_byte_slice(t_path, t_start, t_i));
t_start = (t_i + 1);
}
}
t_i += 1;
}
ctron_list_push((ctron_list*)(t_segs), ctron_byte_slice(t_path, t_start, strlen((const char*)(t_path))));
const char* t_root = ((const char*)((ctron_list*)(t_segs))->items[0]);
if ((!t_gui_ck_has_style(t_pnames, t_root))) {
{
return 0;
}
}
const char* t_cur = "";
int32_t t_ri = 0;
while ((t_ri < ((ctron_list*)(t_pnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_ri])), (const char*)(t_root)) == 0)) {
{
t_cur = t_gui_ty_head_txt(((const char*)((ctron_list*)(t_ptyps))->items[t_ri]));
t_ri = ((ctron_list*)(t_pnames))->n;
}
}
t_ri += 1;
}
if ((!t_gui_ck_has_style(t_stns, t_cur))) {
{
return 0;
}
}
int32_t t_si = 1;
{
const char* t_seg = 0;
int t_last = 0;
const char* t_fkey = 0;
while ((t_si < ((ctron_list*)(t_segs))->n)) {
t_seg = ((const char*)((ctron_list*)(t_segs))->items[t_si]);
t_last = (t_si == (((ctron_list*)(t_segs))->n - 1));
t_fkey = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_cur), (const char*)("."))), (const char*)(t_seg));
if (t_gui_ck_has_style(t_sfk, t_fkey)) {
{
int32_t t_fi = 0;
while ((t_fi < ((ctron_list*)(t_sfk))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sfk))->items[t_fi])), (const char*)(t_fkey)) == 0)) {
{
t_cur = t_gui_ty_head_txt(((const char*)((ctron_list*)(t_stv))->items[t_fi]));
t_fi = ((ctron_list*)(t_sfk))->n;
}
}
t_fi += 1;
}
}
}
else {
{
if ((strcmp((const char*)(t_seg), (const char*)("len")) == 0)) {
{
if (t_last) {
{
return 0;
}
}
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8110: len 后缀须为路径末段 "), (const char*)(t_path))), (const char*)(" 于 "))), (const char*)(t_where)));
return (-1);
}
}
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8110: 字段未声明 "), (const char*)(t_path))), (const char*)("("))), (const char*)(t_cur))), (const char*)(" 无 "))), (const char*)(t_seg))), (const char*)(") 于 "))), (const char*)(t_where)));
return (-1);
}
}
t_si += 1;
}
}
return 0;
}
const char* t_gui_ty_head_node(ctron_list* t_t) 
{
const char* t_tag = ((const char*)((ctron_list*)(t_t))->items[0]);
if ((strcmp((const char*)(t_tag), (const char*)("Named")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_t))->items[1])), (const char*)("Box")) == 0)) {
{
if ((((ctron_list*)(t_t))->n > 2)) {
{
const char* t_ta = ((const char*)((ctron_list*)(t_t))->items[2]);
if ((ctron_len((const void*)(t_ta)) > 1)) {
{
return t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_ta))->items[1])));
}
}
}
}
}
}
return ((const char*)((ctron_list*)(t_t))->items[1]);
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Ref")) == 0)) {
{
return t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_t))->items[1])));
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Optional")) == 0)) {
{
return t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_t))->items[1])));
}
}
return t_tag;
}
int32_t t_gui_ck_bx_gate(const char* t_e, ctron_list* t_props, ctron_list* t_ptyps, ctron_list* t_pscope, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_gdiags, const char* t_where) 
{
if ((((ctron_list*)(t_props))->n == 0)) {
{
return 0;
}
}
int32_t t_bad = 0;
ctron_list* t_paths = (ctron_list*)(ctron_list_new());
t_gui_bx_dotted(t_e, t_paths);
int32_t t_ii = 0;
{
const char* t_p = 0;
const char* t_root = 0;
while ((t_ii < ((ctron_list*)(t_paths))->n)) {
t_p = ((const char*)((ctron_list*)(t_paths))->items[t_ii]);
t_root = t_p;
int32_t t_di = 0;
while ((t_di < strlen((const char*)(t_p)))) {
if ((ctron_byte_at(t_p, t_di) == 46)) {
{
t_root = ctron_byte_slice(t_p, 0, t_di);
t_di = strlen((const char*)(t_p));
}
}
t_di += 1;
}
if (t_gui_ck_has_style(t_pscope, t_root)) {
{
}
}
else {
{
if (t_gui_ck_has_style(t_props, t_root)) {
{
if ((t_gui_ck_prop_path(t_p, t_props, t_ptyps, t_stns, t_sfk, t_stv, t_gdiags, t_where) < 0)) {
{
t_bad = (-1);
}
}
}
}
else {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8110: 绑定引用未声明(须 prop/each 项) "), (const char*)(t_root))), (const char*)(" 于 "))), (const char*)(t_where)));
t_bad = (-1);
}
}
}
}
t_ii += 1;
}
}
return t_bad;
}
int32_t t_gui_ck_color(const char* t_v, ctron_list* t_gdiags) 
{
if ((strlen((const char*)(t_v)) != 7)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8130: 颜色须 #RRGGBB,得 "), (const char*)(t_v)));
return (-1);
}
}
if ((ctron_byte_at(t_v, 0) != 35)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8130: 颜色须 # 开头,得 "), (const char*)(t_v)));
return (-1);
}
}
int32_t t_i = 1;
{
int32_t t_b = 0;
int t_ok = 0;
while ((t_i < 7)) {
t_b = ctron_byte_at(t_v, t_i);
t_ok = ((t_b >= 48) && (t_b <= 57));
if ((!t_ok)) {
{
if ((t_b >= 97)) {
{
if ((t_b <= 102)) {
{
t_ok = 1;
}
}
}
}
}
}
if ((!t_ok)) {
{
if ((t_b >= 65)) {
{
if ((t_b <= 70)) {
{
t_ok = 1;
}
}
}
}
}
}
if ((!t_ok)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8130: 颜色含非十六进制字符,得 "), (const char*)(t_v)));
return (-1);
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_ck_tag(const char* t_tag, ctron_list* t_gdiags) 
{
if ((strcmp((const char*)(t_tag), (const char*)("vbox")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("hbox")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("label")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("button")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("when")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("input")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("checkbox")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("spacer")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("slot")) == 0)) {
{
return 0;
}
}
int32_t t_fb3 = ctron_byte_at(t_tag, 0);
if ((t_fb3 >= 65)) {
{
if ((t_fb3 <= 90)) {
{
return 0;
}
}
}
}
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8100: 未声明标签 "), (const char*)(t_tag)));
return (-1);
}
int32_t t_gui_ck_elem(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_sn, ctron_list* t_fns, ctron_list* t_fna, ctron_list* t_gdiags, ctron_list* t_gwarns, int32_t t_depth, ctron_cell* t_sawin, int t_indep, ctron_list* t_props, ctron_list* t_ptyps, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_pscope, ctron_list* t_vr_nm, ctron_list* t_vr_pp, int t_iscomp, ctron_list* t_fbox) 
{
int32_t t_bad = 0;
if ((t_depth > 8)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: 嵌套超限(限深 8)");
return (-1);
}
}
const char* t_tag = t_eat_tok(t_tks, t_cur);
if ((t_gui_ck_tag(t_tag, t_gdiags) < 0)) {
{
t_bad = (-1);
}
}
if ((strcmp((const char*)(t_tag), (const char*)("input")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_sawin))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_sawin))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
ctron_list* t_cscope = (ctron_list*)(t_pscope);
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_it = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_it), (const char*)(">")) != 0)) {
{
if ((strcmp((const char*)(t_it), (const char*)("/")) != 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_cscope = (ctron_list*)(ctron_list_new());
int32_t t_si = 0;
while ((t_si < ((ctron_list*)(t_pscope))->n)) {
ctron_list_push((ctron_list*)(t_cscope), ((const char*)((ctron_list*)(t_pscope))->items[t_si]));
t_si += 1;
}
ctron_list_push((ctron_list*)(t_cscope), t_it);
}
}
}
}
}
}
}
}
int32_t t_fb4 = ctron_byte_at(t_tag, 0);
if ((t_fb4 >= 65)) {
{
if ((t_fb4 <= 90)) {
{
ctron_list* t_passed = (ctron_list*)(ctron_list_new());
{
const char* t_pk3 = 0;
const char* t_pn2 = 0;
int t_brf = 0;
int t_found = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk3 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk3)) == 1)) {
{
int32_t t_pb3 = ctron_byte_at(t_pk3, 0);
if (t_or2((t_pb3 == 62), (t_pb3 == 47))) {
{
break;
}
}
}
}
t_pn2 = t_eat_tok(t_tks, t_cur);
t_eat_tok(t_tks, t_cur);
t_brf = 0;
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_vw2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_vw2)) == 1)) {
{
if ((ctron_byte_at(t_vw2, 0) == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
const char* t_ev2 = t_gui_bx_expr(t_tks, t_cur);
t_brf = 1;
if ((t_gui_ck_bx_gate(t_ev2, t_props, t_ptyps, t_pscope, t_stns, t_sfk, t_stv, t_gdiags, ctron_str_concat((const char*)("实例prop:"), (const char*)(t_pn2))) < 0)) {
{
t_bad = (-1);
}
}
}
}
else {
{
t_eat_tok(t_tks, t_cur);
}
}
}
}
else {
{
t_eat_tok(t_tks, t_cur);
}
}
}
}
int32_t t_cpi = 0;
t_found = 0;
while ((t_cpi < ((ctron_list*)(t_vr_nm))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vr_nm))->items[t_cpi])), (const char*)(t_tag)) == 0)) {
{
const char* t_cp2 = ((const char*)((ctron_list*)(t_vr_pp))->items[t_cpi]);
int32_t t_ci = 0;
int32_t t_cos = 0;
while ((t_ci <= strlen((const char*)(t_cp2)))) {
if ((t_ci == strlen((const char*)(t_cp2)))) {
{
if ((t_ci > t_cos)) {
{
if ((strcmp((const char*)(ctron_byte_slice(t_cp2, t_cos, t_ci)), (const char*)(t_pn2)) == 0)) {
{
t_found = 1;
}
}
}
}
t_ci += 1;
}
}
else {
{
if ((ctron_byte_at(t_cp2, t_ci) == 1)) {
{
if ((t_ci > t_cos)) {
{
if ((strcmp((const char*)(ctron_byte_slice(t_cp2, t_cos, t_ci)), (const char*)(t_pn2)) == 0)) {
{
t_found = 1;
}
}
}
}
t_cos = (t_ci + 1);
}
}
t_ci += 1;
}
}
}
t_cpi = ((ctron_list*)(t_vr_nm))->n;
}
}
t_cpi += 1;
}
if ((!t_found)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8110: 实例未知 prop "), (const char*)(t_pn2))), (const char*)("(组件 "))), (const char*)(t_tag))), (const char*)(" 未声明)")));
t_bad = (-1);
}
}
ctron_list_push((ctron_list*)(t_passed), t_pn2);
}
}
int t_selfc3 = 0;
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_cw4 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_cw4)) == 1)) {
{
if ((ctron_byte_at(t_cw4, 0) == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_selfc3 = 1;
}
}
}
}
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
int32_t t_cpi2 = 0;
while ((t_cpi2 < ((ctron_list*)(t_vr_nm))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vr_nm))->items[t_cpi2])), (const char*)(t_tag)) == 0)) {
{
const char* t_cp3 = ((const char*)((ctron_list*)(t_vr_pp))->items[t_cpi2]);
int32_t t_ci2 = 0;
int32_t t_cos2 = 0;
while ((t_ci2 <= strlen((const char*)(t_cp3)))) {
if ((t_ci2 == strlen((const char*)(t_cp3)))) {
{
if ((t_ci2 > t_cos2)) {
{
const char* t_want = ctron_byte_slice(t_cp3, t_cos2, t_ci2);
int32_t t_got = 0;
int32_t t_gi2 = 0;
while ((t_gi2 < ((ctron_list*)(t_passed))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_passed))->items[t_gi2])), (const char*)(t_want)) == 0)) {
{
t_got = 1;
}
}
t_gi2 += 1;
}
if ((t_got == 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8100: 实例缺 prop "), (const char*)(t_want))), (const char*)("(组件 "))), (const char*)(t_tag))), (const char*)(")")));
t_bad = (-1);
}
}
}
}
t_ci2 += 1;
}
}
else {
{
if ((ctron_byte_at(t_cp3, t_ci2) == 1)) {
{
t_cos2 = (t_ci2 + 1);
}
}
t_ci2 += 1;
}
}
}
t_cpi2 = ((ctron_list*)(t_vr_nm))->n;
}
}
t_cpi2 += 1;
}
if ((!t_selfc3)) {
{
{
const char* t_pk4 = 0;
int32_t t_cb2 = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk4 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk4)) == 1)) {
{
if ((ctron_byte_at(t_pk4, 0) == 60)) {
{
if (((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_nk3 = ((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
if ((strlen((const char*)(t_nk3)) == 1)) {
{
if ((ctron_byte_at(t_nk3, 0) == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 2)), __ATOMIC_SEQ_CST);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_et3 = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_et3), (const char*)(t_tag)) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8100: 闭合标签不匹配 "), (const char*)(t_et3)));
t_bad = (-1);
}
}
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_gt2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_gt2)) == 1)) {
{
if ((ctron_byte_at(t_gt2, 0) == 62)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
}
}
break;
}
}
}
}
}
}
}
}
}
}
t_cb2 = t_gui_ck_elem(t_tks, t_cur, t_sn, t_fns, t_fna, t_gdiags, t_gwarns, (t_depth + 1), t_sawin, t_indep, t_props, t_ptyps, t_stns, t_sfk, t_stv, t_cscope, t_vr_nm, t_vr_pp, t_iscomp, t_fbox);
if ((t_cb2 < 0)) {
{
t_bad = (-1);
}
}
}
}
}
}
return t_bad;
}
}
}
}
int t_has_cond = 0;
int t_has_bind = 0;
int t_has_in = 0;
int t_has_checked = 0;
int t_selfc = 0;
{
const char* t_pk = 0;
const char* t_an = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk)) == 1)) {
{
int32_t t_pb = ctron_byte_at(t_pk, 0);
if ((t_pb == 62)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
break;
}
}
if ((t_pb == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_selfc = 1;
break;
}
}
}
}
t_an = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_an), (const char*)("on")) == 0)) {
{
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)(":")) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: on 期待冒号");
t_bad = (-1);
}
}
const char* t_evn = t_eat_tok(t_tks, t_cur);
const char* t_hn = "";
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("=")) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: on 期待等号");
t_bad = (-1);
}
}
else {
{
const char* t_ob = t_eat_tok(t_tks, t_cur);
if ((strlen((const char*)(t_ob)) != 1)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: on 期待块开");
t_bad = (-1);
}
}
else {
{
if ((ctron_byte_at(t_ob, 0) != 123)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: on 期待块开");
t_bad = (-1);
}
}
else {
{
t_hn = t_gui_bx_expr(t_tks, t_cur);
}
}
}
}
}
}
if ((strcmp((const char*)(t_hn), (const char*)("")) == 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: on 事件表达式为空");
t_bad = (-1);
}
}
if ((strcmp((const char*)(t_hn), (const char*)("")) != 0)) {
{
ctron_list* t_argc = (ctron_list*)(ctron_list_new());
ctron_list* t_aout = (ctron_list*)(ctron_list_new());
const char* t_head = t_gui_ev_head2(t_hn, t_argc, t_aout);
if ((((int32_t)(long)((ctron_list*)(t_argc))->items[0]) == (-2))) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8110: on: 事件表达式仅路径或调用形态 "), (const char*)(t_hn))), (const char*)("(on:"))), (const char*)(t_evn))), (const char*)(")")));
t_bad = (-1);
}
}
else {
{
if ((((int32_t)(long)((ctron_list*)(t_argc))->items[0]) == (-1))) {
{
if ((!t_indep)) {
{
if ((!t_gui_ck_has_style(t_fns, t_hn))) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8120: 事件处理器未声明 "), (const char*)(t_hn))), (const char*)("(on:"))), (const char*)(t_evn))), (const char*)(")")));
t_bad = (-1);
}
}
}
}
}
}
else {
{
if ((!t_indep)) {
{
int32_t t_da = t_gui_ck_fn_argc(t_fns, t_fna, t_head);
int32_t t_imp = 0;
if ((strcmp((const char*)(t_evn), (const char*)("input")) == 0)) {
{
if ((strcmp((const char*)(t_tag), (const char*)("input")) == 0)) {
{
t_imp = 1;
}
}
}
}
if ((((ctron_list*)(t_pscope))->n > 0)) {
{
t_imp += 1;
}
}
if (t_iscomp) {
{
if (t_gui_ck_has_style(t_fbox, t_head)) {
{
t_imp += 1;
}
}
}
}
if ((t_da < 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8120: 事件处理器未声明 "), (const char*)(t_head))), (const char*)("(on:"))), (const char*)(t_evn))), (const char*)(")")));
t_bad = (-1);
}
}
else {
{
if ((t_da != (((int32_t)(long)((ctron_list*)(t_argc))->items[0]) + t_imp))) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8120: 事件签名不匹配 "), (const char*)(t_head))), (const char*)("(实参 "))), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_argc))->items[0])))))), (const char*)(",声明 "))), (const char*)(ctron_i32_to_string((int32_t)(t_da))))), (const char*)(")(on:"))), (const char*)(t_evn))), (const char*)(")")));
t_bad = (-1);
}
}
}
}
}
}
if ((t_gui_ck_bx_gate(((const char*)((ctron_list*)(t_aout))->items[0]), t_props, t_ptyps, t_pscope, t_stns, t_sfk, t_stv, t_gdiags, ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("on:"), (const char*)(t_evn))), (const char*)("={"))), (const char*)(t_hn))), (const char*)("}"))) < 0)) {
{
t_bad = (-1);
}
}
}
}
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("=")) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: 属性期待等号");
t_bad = (-1);
}
}
else {
{
const char* t_av = "";
int t_brform = 0;
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_pv = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pv)) == 1)) {
{
if ((ctron_byte_at(t_pv, 0) == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_av = t_gui_bx_expr(t_tks, t_cur);
if ((strcmp((const char*)(t_av), (const char*)("")) == 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: 花括号属性期待表达式");
t_bad = (-1);
}
}
t_brform = 1;
if (t_gui_bx_impure(t_av)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8190: 绑定表达式纯度违规(赋值/自增减) "), (const char*)(t_an))), (const char*)("={"))), (const char*)(t_av))), (const char*)("}")));
t_bad = (-1);
}
}
if ((t_gui_ck_bx_gate(t_av, t_props, t_ptyps, t_pscope, t_stns, t_sfk, t_stv, t_gdiags, ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_an), (const char*)("={"))), (const char*)(t_av))), (const char*)("}"))) < 0)) {
{
t_bad = (-1);
}
}
}
}
}
}
}
}
if ((!t_brform)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
t_av = t_gui_unq(t_eat_tok(t_tks, t_cur));
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("class")) == 0)) {
{
if ((!t_gui_ck_has_style(t_sn, t_av))) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8150: class 引用未定义样式 "), (const char*)(t_av)));
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("cond")) == 0)) {
{
t_has_cond = 1;
if ((!t_brform)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8110: cond 须花括号绑定名形态,得 "), (const char*)(t_av)));
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("bind")) == 0)) {
{
t_has_bind = 1;
if ((!t_brform)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8110: bind 须花括号绑定名形态,得 "), (const char*)(t_av)));
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("in")) == 0)) {
{
t_has_in = 1;
if ((!t_brform)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8110: in 须花括号绑定名形态,得 "), (const char*)(t_av)));
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("checked")) == 0)) {
{
t_has_checked = 1;
if ((!t_brform)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8110: checked 须花括号绑定名形态,得 "), (const char*)(t_av)));
t_bad = (-1);
}
}
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(t_tag), (const char*)("when")) == 0)) {
{
if ((!t_has_cond)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: when 缺 cond");
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_tag), (const char*)("input")) == 0)) {
{
if ((!t_has_bind)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: input 缺 bind");
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_tag), (const char*)("checkbox")) == 0)) {
{
if ((!t_has_checked)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: checkbox 缺 checked");
t_bad = (-1);
}
}
}
}
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
if ((!t_has_in)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: each 缺 in");
t_bad = (-1);
}
}
}
}
if (t_selfc) {
{
return t_bad;
}
}
int32_t t_in_before = __atomic_load_n(&((ctron_cell*)(t_sawin))->v, __ATOMIC_SEQ_CST);
{
const char* t_pk2 = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk2)) == 1)) {
{
int32_t t_b2 = ctron_byte_at(t_pk2, 0);
if ((t_b2 == 60)) {
{
if (((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_nk = ((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
if ((strlen((const char*)(t_nk)) == 1)) {
{
if ((ctron_byte_at(t_nk, 0) == 47)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 2)), __ATOMIC_SEQ_CST);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_et = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_et), (const char*)(t_tag)) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8100: 闭合标签不匹配 "), (const char*)(t_et)));
t_bad = (-1);
}
}
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_gt = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_gt)) == 1)) {
{
if ((ctron_byte_at(t_gt, 0) == 62)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
}
}
break;
}
}
}
}
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
int32_t t_cb = t_gui_ck_elem(t_tks, t_cur, t_sn, t_fns, t_fna, t_gdiags, t_gwarns, (t_depth + 1), t_sawin, t_indep, t_props, t_ptyps, t_stns, t_sfk, t_stv, t_cscope, t_vr_nm, t_vr_pp, t_iscomp, t_fbox);
if ((t_cb < 0)) {
{
t_bad = (-1);
}
}
}
}
else {
{
if ((t_b2 == 35)) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8100: 未终止 "), (const char*)(t_tag)));
t_bad = (-1);
break;
}
}
if ((t_b2 == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
const char* t_lv = t_gui_bx_expr(t_tks, t_cur);
if ((t_gui_ck_bx_gate(t_lv, t_props, t_ptyps, t_pscope, t_stns, t_sfk, t_stv, t_gdiags, "叶") < 0)) {
{
t_bad = (-1);
}
}
continue;
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
else {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_sawin))->v, __ATOMIC_SEQ_CST) > t_in_before)) {
{
ctron_list_push((ctron_list*)(t_gwarns), "E8193: each 体含 input,索引对齐重排将串位(key 原语未落地)");
}
}
}
}
return t_bad;
}
int32_t t_gui_props_names(const char* t_d5, ctron_list* t_out) 
{
if ((strlen((const char*)(t_d5)) > 2)) {
{
const char* t_seg = ctron_byte_slice(t_d5, 2, (strlen((const char*)(t_d5)) - 2));
int32_t t_pi = 0;
{
int32_t t_ps = 0;
const char* t_unit = 0;
while ((t_pi < strlen((const char*)(t_seg)))) {
t_ps = t_pi;
while ((t_pi < strlen((const char*)(t_seg)))) {
if ((ctron_byte_at(t_seg, t_pi) == 44)) {
{
break;
}
}
t_pi += 1;
}
t_unit = ctron_byte_slice(t_seg, t_ps, t_pi);
int32_t t_ui = 0;
while ((t_ui < strlen((const char*)(t_unit)))) {
if ((ctron_byte_at(t_unit, t_ui) == 58)) {
{
const char* t_pn = ctron_byte_slice(t_unit, 0, t_ui);
int32_t t_pe = strlen((const char*)(t_pn));
{
int32_t t_pc = 0;
while ((t_pe > 0)) {
t_pc = ctron_byte_at(t_pn, (t_pe - 1));
if (((t_pc == 32) || (t_pc == 9))) {
{
t_pe -= 1;
}
}
else {
{
break;
}
}
}
}
int32_t t_psi = 0;
while ((t_psi < t_pe)) {
if (((ctron_byte_at(t_pn, t_psi) != 32) && (ctron_byte_at(t_pn, t_psi) != 9))) {
{
break;
}
}
t_psi += 1;
}
if ((t_pe > t_psi)) {
{
ctron_list_push((ctron_list*)(t_out), ctron_byte_slice(t_pn, t_psi, t_pe));
}
}
t_ui = strlen((const char*)(t_unit));
}
}
t_ui += 1;
}
t_pi += 1;
}
}
}
}
return 0;
}
int32_t t_gui_check_file(ctron_list* t_file, ctron_list* t_gdiags, ctron_list* t_gwarns, int t_indep) 
{
ctron_list* t_sn = (ctron_list*)(ctron_list_new());
ctron_list* t_fns = (ctron_list*)(ctron_list_new());
ctron_list* t_fna = (ctron_list*)(ctron_list_new());
ctron_list* t_sext = (ctron_list*)(ctron_list_new());
ctron_list* t_vr_nm = (ctron_list*)(ctron_list_new());
ctron_list* t_vr_pp = (ctron_list*)(ctron_list_new());
const char* t_tgt = t_gui_ds_marker_file(t_file);
ctron_list* t_fbox = (ctron_list*)(ctron_list_new());
int32_t t_vri = 1;
{
const char* t_vd = 0;
while ((t_vri < ((ctron_list*)(t_file))->n)) {
t_vd = ((const char*)((ctron_list*)(t_file))->items[t_vri]);
if (((((strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[0])), (const char*)("Fn")) == 0) || (strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[0])), (const char*)("FnPub")) == 0)) || (strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[0])), (const char*)("FnExt")) == 0)) || (strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[0])), (const char*)("FnC")) == 0))) {
{
if ((ctron_len((const void*)(t_vd)) > 3)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_vd))->items[3]))) > 1)) {
{
const char* t_p0 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vd))->items[3])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p0))->items[0])), (const char*)("Param")) == 0)) {
{
if ((ctron_len((const void*)(t_p0)) > 3)) {
{
if (t_gui_ty_is_box((ctron_list*)(((const char*)((ctron_list*)(t_p0))->items[3])))) {
{
ctron_list_push((ctron_list*)(t_fbox), ((const char*)((ctron_list*)(t_vd))->items[1]));
}
}
}
}
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vd))->items[1])), (const char*)("view")) == 0)) {
{
ctron_list_push((ctron_list*)(t_vr_nm), ((const char*)((ctron_list*)(t_vd))->items[2]));
ctron_list* t_pp2 = (ctron_list*)(ctron_list_new());
if ((ctron_len((const void*)(t_vd)) > 5)) {
{
t_gui_props_names(((const char*)((ctron_list*)(t_vd))->items[5]), t_pp2);
}
}
const char* t_joined = "";
int32_t t_pj = 0;
while ((t_pj < ((ctron_list*)(t_pp2))->n)) {
if ((t_pj > 0)) {
{
t_joined = ctron_str_concat((const char*)(t_joined), (const char*)(ctron_utf8_enc((int)(1))));
}
}
t_joined = ctron_str_concat((const char*)(t_joined), (const char*)(((const char*)((ctron_list*)(t_pp2))->items[t_pj])));
t_pj += 1;
}
ctron_list_push((ctron_list*)(t_vr_pp), t_joined);
}
}
}
}
t_vri += 1;
}
}
ctron_list* t_stns = (ctron_list*)(ctron_list_new());
ctron_list* t_sfk = (ctron_list*)(ctron_list_new());
ctron_list* t_stv = (ctron_list*)(ctron_list_new());
int32_t t_si = 1;
{
const char* t_ds = 0;
while ((t_si < ((ctron_list*)(t_file))->n)) {
t_ds = ((const char*)((ctron_list*)(t_file))->items[t_si]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[1])), (const char*)("style")) == 0)) {
{
ctron_list* t_stks = (ctron_list*)(ctron_list_new());
t_gui_split(((const char*)((ctron_list*)(t_ds))->items[4]), t_stks);
ctron_list_push((ctron_list*)(t_sn), ((const char*)((ctron_list*)(t_stks))->items[0]));
if ((((ctron_list*)(t_stks))->n > 2)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_stks))->items[1])), (const char*)("extends")) == 0)) {
{
ctron_list_push((ctron_list*)(t_sext), ((const char*)((ctron_list*)(t_stks))->items[0]));
ctron_list_push((ctron_list*)(t_sext), ((const char*)((ctron_list*)(t_stks))->items[2]));
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[0])), (const char*)("Fn")) == 0)) {
{
if ((ctron_len((const void*)(t_ds)) > 1)) {
{
ctron_list_push((ctron_list*)(t_fns), ((const char*)((ctron_list*)(t_ds))->items[1]));
ctron_list_push((ctron_list*)(t_fna), (const char*)(long)(t_gui_fn_arity((ctron_list*)(t_ds))));
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[0])), (const char*)("Struct")) == 0)) {
{
if ((ctron_len((const void*)(t_ds)) > 4)) {
{
const char* t_fsx = ((const char*)((ctron_list*)(t_ds))->items[4]);
int32_t t_fi = 1;
{
const char* t_fd = 0;
while ((t_fi < ctron_len((const void*)(t_fsx)))) {
t_fd = ((const char*)((ctron_list*)(t_fsx))->items[t_fi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_fd))->items[0])), (const char*)("Field")) == 0)) {
{
ctron_list_push((ctron_list*)(t_stns), ((const char*)((ctron_list*)(t_ds))->items[1]));
ctron_list_push((ctron_list*)(t_sfk), ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_ds))->items[1])), (const char*)("."))), (const char*)(((const char*)((ctron_list*)(t_fd))->items[2]))));
ctron_list_push((ctron_list*)(t_stv), t_gui_ty_head_node((ctron_list*)(((const char*)((ctron_list*)(t_fd))->items[3]))));
}
}
t_fi += 1;
}
}
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ds))->items[0])), (const char*)("FnExt")) == 0)) {
{
if ((ctron_len((const void*)(t_ds)) > 1)) {
{
ctron_list_push((ctron_list*)(t_fns), ((const char*)((ctron_list*)(t_ds))->items[1]));
ctron_list_push((ctron_list*)(t_fna), (const char*)(long)(t_gui_fn_arity((ctron_list*)(t_ds))));
}
}
}
}
t_si += 1;
}
}
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
ctron_list* t_tks = (ctron_list*)(ctron_list_new());
t_gui_split(((const char*)((ctron_list*)(t_d))->items[4]), t_tks);
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
ctron_list* t_vprops = (ctron_list*)(ctron_list_new());
ctron_list* t_vptyps = (ctron_list*)(ctron_list_new());
if ((ctron_len((const void*)(t_d)) > 5)) {
{
t_gui_props_typed(((const char*)((ctron_list*)(t_d))->items[5]), t_vprops, t_vptyps);
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)(1), __ATOMIC_SEQ_CST);
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)("<")) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8100: view 期待 <");
}
}
else {
{
ctron_cell* t_sawin = (ctron_cell*)(ctron_cell_new(0));
ctron_list* t_emptyscope = (ctron_list*)(ctron_list_new());
int t_iscomp = 0;
if ((strcmp((const char*)(t_tgt), (const char*)("")) != 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[2])), (const char*)(t_tgt)) != 0)) {
{
t_iscomp = 1;
}
}
}
}
t_gui_ck_elem(t_tks, t_cur, t_sn, t_fns, t_fna, t_gdiags, t_gwarns, 1, t_sawin, t_indep, t_vprops, t_vptyps, t_stns, t_sfk, t_stv, t_emptyscope, t_vr_nm, t_vr_pp, t_iscomp, t_fbox);
}
}
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("style")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)(1), __ATOMIC_SEQ_CST);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)("extends")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 2)), __ATOMIC_SEQ_CST);
}
}
}
}
{
const char* t_pp = 0;
const char* t_vv = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pp = t_eat_tok(t_tks, t_cur);
if ((strcmp((const char*)(t_eat_tok(t_tks, t_cur)), (const char*)(":")) != 0)) {
{
ctron_list_push((ctron_list*)(t_gdiags), "E8130: style 期待冒号");
}
}
t_vv = t_gui_unq(t_eat_tok(t_tks, t_cur));
if ((strcmp((const char*)(t_pp), (const char*)("bg")) == 0)) {
{
t_gui_ck_color(t_vv, t_gdiags);
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)])), (const char*)(";")) == 0)) {
{
t_eat_tok(t_tks, t_cur);
}
}
}
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
int32_t t_xi = 0;
while ((t_xi < ((ctron_list*)(t_sext))->n)) {
if ((!t_gui_ck_has_style(t_sn, ((const char*)((ctron_list*)(t_sext))->items[(t_xi + 1)])))) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E8100: extends 父样式未声明 "), (const char*)(((const char*)((ctron_list*)(t_sext))->items[t_xi])))), (const char*)(" extends "))), (const char*)(((const char*)((ctron_list*)(t_sext))->items[(t_xi + 1)]))));
}
}
t_xi += 2;
}
t_xi = 0;
{
const char* t_cur2 = 0;
int t_cyc = 0;
while ((t_xi < ((ctron_list*)(t_sext))->n)) {
t_cur2 = ((const char*)((ctron_list*)(t_sext))->items[t_xi]);
int32_t t_step = 0;
t_cyc = 1;
{
int t_found = 0;
while ((t_step <= ((ctron_list*)(t_sext))->n)) {
t_found = 0;
int32_t t_pi = 0;
while ((t_pi < ((ctron_list*)(t_sext))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sext))->items[t_pi])), (const char*)(t_cur2)) == 0)) {
{
t_cur2 = ((const char*)((ctron_list*)(t_sext))->items[(t_pi + 1)]);
t_found = 1;
t_pi = ((ctron_list*)(t_sext))->n;
}
}
t_pi += 2;
}
if ((!t_found)) {
{
t_cyc = 0;
t_step = (((ctron_list*)(t_sext))->n + 1);
}
}
t_step += 1;
}
}
if (t_cyc) {
{
ctron_list_push((ctron_list*)(t_gdiags), ctron_str_concat((const char*)("E8100: 样式 extends 环 "), (const char*)(((const char*)((ctron_list*)(t_sext))->items[t_xi]))));
}
}
t_xi += 2;
}
}
return 0;
}
const char* t_gui_sk_word(ctron_list* t_tks, ctron_cell* t_cur) 
{
const char* t_t = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
if ((strlen((const char*)(t_t)) > 1)) {
{
if ((ctron_byte_at(t_t, (strlen((const char*)(t_t)) - 1)) == 126)) {
{
t_t = ctron_byte_slice(t_t, 0, (strlen((const char*)(t_t)) - 1));
}
}
}
}
return t_t;
}
const char* t_gui_bx_expr(ctron_list* t_tks, ctron_cell* t_cur) 
{
const char* t_out = "";
int32_t t_depth = 1;
{
const char* t_t = 0;
int t_isstr = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_t = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
if ((strlen((const char*)(t_t)) > 1)) {
{
if ((ctron_byte_at(t_t, (strlen((const char*)(t_t)) - 1)) == 126)) {
{
t_t = ctron_byte_slice(t_t, 0, (strlen((const char*)(t_t)) - 1));
}
}
}
}
t_isstr = 0;
if ((strlen((const char*)(t_t)) > 0)) {
{
if ((ctron_byte_at(t_t, 0) == 34)) {
{
t_isstr = 1;
}
}
}
}
if ((!t_isstr)) {
{
if ((strlen((const char*)(t_t)) == 1)) {
{
int32_t t_tb = ctron_byte_at(t_t, 0);
if ((t_tb == 123)) {
{
t_depth += 1;
}
}
else {
{
if ((t_tb == 125)) {
{
t_depth -= 1;
if ((t_depth == 0)) {
{
return t_out;
}
}
}
}
}
}
}
}
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_t));
}
}
return t_out;
}
int t_gui_bx_impure(const char* t_e) 
{
int32_t t_i = 0;
int t_instr = 0;
{
int32_t t_b = 0;
while ((t_i < strlen((const char*)(t_e)))) {
t_b = ctron_byte_at(t_e, t_i);
if (t_instr) {
{
if ((t_b == 92)) {
{
t_i += 1;
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 0;
}
}
}
}
}
}
else {
{
if ((t_b == 34)) {
{
t_instr = 1;
}
}
else {
{
if ((t_b == 61)) {
{
int t_p = 0;
if ((t_i > 0)) {
{
int32_t t_pb = ctron_byte_at(t_e, (t_i - 1));
if (((((t_pb == 61) || (t_pb == 33)) || (t_pb == 60)) || (t_pb == 62))) {
{
t_p = 1;
}
}
}
}
int t_q = 0;
if (((t_i + 1) < strlen((const char*)(t_e)))) {
{
if ((ctron_byte_at(t_e, (t_i + 1)) == 61)) {
{
t_q = 1;
}
}
}
}
if ((!t_p)) {
{
if ((!t_q)) {
{
return 1;
}
}
}
}
}
}
else {
{
if (((t_b == 43) || (t_b == 45))) {
{
if (((t_i + 1) < strlen((const char*)(t_e)))) {
{
int32_t t_nb = ctron_byte_at(t_e, (t_i + 1));
if ((t_nb == t_b)) {
{
return 1;
}
}
if ((t_nb == 61)) {
{
return 1;
}
}
}
}
}
}
else {
{
if ((((t_b == 42) || (t_b == 47)) || (t_b == 37))) {
{
if (((t_i + 1) < strlen((const char*)(t_e)))) {
{
if ((ctron_byte_at(t_e, (t_i + 1)) == 61)) {
{
return 1;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_sk_attrs(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_out4) 
{
{
const char* t_pk = 0;
const char* t_an = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk)) == 1)) {
{
int32_t t_pb = ctron_byte_at(t_pk, 0);
if ((t_pb == 62)) {
{
break;
}
}
if ((t_pb == 47)) {
{
break;
}
}
}
}
t_an = t_gui_sk_word(t_tks, t_cur);
if ((strcmp((const char*)(t_an), (const char*)("on")) == 0)) {
{
t_gui_sk_word(t_tks, t_cur);
const char* t_evn = t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
const char* t_ob = t_gui_sk_word(t_tks, t_cur);
if ((strlen((const char*)(t_ob)) != 1)) {
{
ctron_panic("gui: on 期待块开");
}
}
if ((ctron_byte_at(t_ob, 0) != 123)) {
{
ctron_panic("gui: on 期待块开");
}
}
const char* t_fnn = t_gui_bx_expr(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ev_name), t_evn);
ctron_list_push((ctron_list*)(t_ev_fn), t_fnn);
}
}
else {
{
t_gui_sk_word(t_tks, t_cur);
const char* t_av = "";
int t_brform = 0;
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_pv = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pv)) == 1)) {
{
if ((ctron_byte_at(t_pv, 0) == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_av = t_gui_bx_expr(t_tks, t_cur);
t_brform = 1;
}
}
}
}
}
}
if ((!t_brform)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
t_av = t_gui_unq(t_gui_sk_word(t_tks, t_cur));
}
}
}
}
if ((strcmp((const char*)(t_an), (const char*)("class")) == 0)) {
{
((ctron_list*)(t_out4))->items[0] = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("cond")) == 0)) {
{
((ctron_list*)(t_out4))->items[1] = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("bind")) == 0)) {
{
((ctron_list*)(t_out4))->items[2] = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("checked")) == 0)) {
{
((ctron_list*)(t_out4))->items[2] = t_av;
}
}
if ((strcmp((const char*)(t_an), (const char*)("placeholder")) == 0)) {
{
((ctron_list*)(t_out4))->items[3] = t_av;
}
}
}
}
}
}
return 0;
}
int32_t t_gui_sk_node(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_ntag, ctron_list* t_nflag, ctron_list* t_ncls, ctron_list* t_npre, ctron_list* t_nbid, ctron_list* t_npost, ctron_list* t_nfc, ctron_list* t_ns, ctron_list* t_nes, ctron_list* t_nec, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_btns, int32_t t_depth) 
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
const char* t_tag = t_gui_sk_word(t_tks, t_cur);
int t_is_leaf = 0;
if ((strcmp((const char*)(t_tag), (const char*)("label")) == 0)) {
{
t_is_leaf = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("button")) == 0)) {
{
t_is_leaf = 1;
}
}
int t_is_input = 0;
if ((strcmp((const char*)(t_tag), (const char*)("input")) == 0)) {
{
t_is_input = 1;
}
}
int t_is_check = 0;
if ((strcmp((const char*)(t_tag), (const char*)("checkbox")) == 0)) {
{
t_is_check = 1;
}
}
const char* t_itemvar = "";
const char* t_listname = "";
if ((strcmp((const char*)(t_tag), (const char*)("each")) == 0)) {
{
t_itemvar = t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
const char* t_ob = t_gui_sk_word(t_tks, t_cur);
if ((strlen((const char*)(t_ob)) != 1)) {
{
ctron_panic("gui: each 期待花括号列表");
}
}
if ((ctron_byte_at(t_ob, 0) != 123)) {
{
ctron_panic("gui: each 期待花括号列表");
}
}
t_listname = t_gui_bx_expr(t_tks, t_cur);
}
}
int32_t t_estart = ((ctron_list*)(t_ev_name))->n;
int32_t t_fb2 = ctron_byte_at(t_tag, 0);
if ((t_fb2 >= 65)) {
{
if ((t_fb2 <= 90)) {
{
const char* t_argtxt = "";
{
const char* t_pk2 = 0;
const char* t_vw = 0;
int t_is_br = 0;
int t_is_q = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk2)) == 1)) {
{
int32_t t_pb2 = ctron_byte_at(t_pk2, 0);
if (t_or2((t_pb2 == 62), (t_pb2 == 47))) {
{
break;
}
}
}
}
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) >= ((ctron_list*)(t_tks))->n)) {
{
break;
}
}
t_vw = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
t_is_br = 0;
t_is_q = 0;
if ((strlen((const char*)(t_vw)) == 1)) {
{
int32_t t_vb2 = ctron_byte_at(t_vw, 0);
if ((t_vb2 == 123)) {
{
t_is_br = 1;
}
}
if ((t_vb2 == 34)) {
{
t_is_q = 1;
}
}
}
}
if (t_is_br) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
const char* t_ex2 = t_gui_bx_expr(t_tks, t_cur);
if ((strcmp((const char*)(t_argtxt), (const char*)("")) != 0)) {
{
t_argtxt = ctron_str_concat((const char*)(t_argtxt), (const char*)(ctron_utf8_enc((int)(1))));
}
}
t_argtxt = ctron_str_concat((const char*)(t_argtxt), (const char*)(t_ex2));
}
}
else {
{
if (t_is_q) {
{
t_gui_sk_word(t_tks, t_cur);
}
}
else {
{
t_gui_sk_word(t_tks, t_cur);
}
}
}
}
}
}
int t_selfc = 0;
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_cw2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_cw2)) == 1)) {
{
if ((ctron_byte_at(t_cw2, 0) == 47)) {
{
t_gui_sk_word(t_tks, t_cur);
t_selfc = 1;
}
}
}
}
}
}
t_gui_sk_word(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ntag), ctron_str_concat((const char*)("call:"), (const char*)(t_tag)));
ctron_list_push((ctron_list*)(t_ncls), "");
ctron_list_push((ctron_list*)(t_npre), "");
ctron_list_push((ctron_list*)(t_nbid), "");
ctron_list_push((ctron_list*)(t_npost), t_argtxt);
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)(0));
ctron_list_push((ctron_list*)(t_nflag), "");
int32_t t_vid2 = (((ctron_list*)(t_ntag))->n - 1);
if ((!t_selfc)) {
{
int32_t t_prev2 = (-1);
{
const char* t_nk = 0;
int32_t t_kid = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_nk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_nk)) == 1)) {
{
if ((ctron_byte_at(t_nk, 0) == 60)) {
{
if (((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_nk2 = ((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
if ((strlen((const char*)(t_nk2)) == 1)) {
{
if ((ctron_byte_at(t_nk2, 0) == 47)) {
{
break;
}
}
}
}
}
}
}
}
}
}
t_kid = t_gui_sk_node(t_tks, t_cur, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_ev_name, t_ev_fn, t_btns, (t_depth + 1));
if ((t_prev2 < 0)) {
{
((ctron_list*)(t_nfc))->items[t_vid2] = (const char*)(long)(t_kid);
}
}
else {
{
((ctron_list*)(t_ns))->items[t_prev2] = (const char*)(long)(t_kid);
}
}
t_prev2 = t_kid;
}
}
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
}
}
return t_vid2;
}
}
}
}
ctron_list* t_out4 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_out4), "");
ctron_list_push((ctron_list*)(t_out4), "");
ctron_list_push((ctron_list*)(t_out4), "");
ctron_list_push((ctron_list*)(t_out4), "");
t_gui_sk_attrs(t_tks, t_cur, t_ev_name, t_ev_fn, t_out4);
int32_t t_id = ((ctron_list*)(t_ntag))->n;
if (t_is_input) {
{
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ntag), t_tag);
ctron_list_push((ctron_list*)(t_ncls), ((const char*)((ctron_list*)(t_out4))->items[0]));
ctron_list_push((ctron_list*)(t_npre), ((const char*)((ctron_list*)(t_out4))->items[3]));
ctron_list_push((ctron_list*)(t_nbid), ((const char*)((ctron_list*)(t_out4))->items[2]));
ctron_list_push((ctron_list*)(t_npost), "");
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)((((ctron_list*)(t_ev_name))->n - t_estart)));
ctron_list_push((ctron_list*)(t_nflag), "");
return t_id;
}
}
if (t_is_check) {
{
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ntag), t_tag);
ctron_list_push((ctron_list*)(t_ncls), ((const char*)((ctron_list*)(t_out4))->items[0]));
ctron_list_push((ctron_list*)(t_npre), "");
ctron_list_push((ctron_list*)(t_nbid), ((const char*)((ctron_list*)(t_out4))->items[2]));
ctron_list_push((ctron_list*)(t_npost), "");
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)((((ctron_list*)(t_ev_name))->n - t_estart)));
ctron_list_push((ctron_list*)(t_nflag), "");
ctron_list_push((ctron_list*)(t_btns), (const char*)(long)(t_id));
return t_id;
}
}
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_sk1 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_sk1)) == 1)) {
{
if ((ctron_byte_at(t_sk1, 0) == 47)) {
{
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ntag), t_tag);
ctron_list_push((ctron_list*)(t_ncls), ((const char*)((ctron_list*)(t_out4))->items[0]));
ctron_list_push((ctron_list*)(t_npre), t_listname);
ctron_list_push((ctron_list*)(t_nbid), t_itemvar);
ctron_list_push((ctron_list*)(t_npost), "");
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)((((ctron_list*)(t_ev_name))->n - t_estart)));
ctron_list_push((ctron_list*)(t_nflag), ((const char*)((ctron_list*)(t_out4))->items[1]));
return t_id;
}
}
}
}
}
}
t_gui_sk_word(t_tks, t_cur);
if (t_is_leaf) {
{
const char* t_p0 = "";
const char* t_b = "";
const char* t_p1 = "";
int32_t t_phase = 0;
{
const char* t_tk = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_tk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_tk)) == 1)) {
{
int32_t t_tb = ctron_byte_at(t_tk, 0);
if ((t_tb == 123)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
t_b = t_gui_bx_expr(t_tks, t_cur);
t_phase = 2;
continue;
}
}
if ((t_tb == 60)) {
{
break;
}
}
}
}
if ((t_phase == 0)) {
{
if ((strcmp((const char*)(t_p0), (const char*)("")) != 0)) {
{
t_p0 = ctron_str_concat((const char*)(t_p0), (const char*)(" "));
}
}
t_p0 = ctron_str_concat((const char*)(t_p0), (const char*)(t_tk));
}
}
else {
{
if ((strcmp((const char*)(t_p1), (const char*)("")) != 0)) {
{
t_p1 = ctron_str_concat((const char*)(t_p1), (const char*)(" "));
}
}
t_p1 = ctron_str_concat((const char*)(t_p1), (const char*)(t_tk));
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
ctron_list_push((ctron_list*)(t_npre), t_p0);
ctron_list_push((ctron_list*)(t_nbid), t_b);
ctron_list_push((ctron_list*)(t_npost), t_p1);
ctron_list_push((ctron_list*)(t_ntag), t_tag);
ctron_list_push((ctron_list*)(t_ncls), ((const char*)((ctron_list*)(t_out4))->items[0]));
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)((((ctron_list*)(t_ev_name))->n - t_estart)));
ctron_list_push((ctron_list*)(t_nflag), ((const char*)((ctron_list*)(t_out4))->items[1]));
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
if ((strcmp((const char*)(t_tag), (const char*)("button")) == 0)) {
{
ctron_list_push((ctron_list*)(t_btns), (const char*)(long)(t_id));
}
}
return t_id;
}
}
ctron_list_push((ctron_list*)(t_ntag), t_tag);
ctron_list_push((ctron_list*)(t_ncls), ((const char*)((ctron_list*)(t_out4))->items[0]));
ctron_list_push((ctron_list*)(t_npre), t_listname);
ctron_list_push((ctron_list*)(t_nbid), t_itemvar);
ctron_list_push((ctron_list*)(t_npost), "");
ctron_list_push((ctron_list*)(t_nfc), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_ns), (const char*)(long)((-1)));
ctron_list_push((ctron_list*)(t_nes), (const char*)(long)(t_estart));
ctron_list_push((ctron_list*)(t_nec), (const char*)(long)((((ctron_list*)(t_ev_name))->n - t_estart)));
ctron_list_push((ctron_list*)(t_nflag), ((const char*)((ctron_list*)(t_out4))->items[1]));
int32_t t_prev = (-1);
{
const char* t_pk2 = 0;
int32_t t_kid = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk2 = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk2)) == 1)) {
{
if ((ctron_byte_at(t_pk2, 0) == 60)) {
{
if (((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_nk = ((const char*)((ctron_list*)(t_tks))->items[(__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)]);
if ((strlen((const char*)(t_nk)) == 1)) {
{
if ((ctron_byte_at(t_nk, 0) == 47)) {
{
break;
}
}
}
}
}
}
}
}
}
}
t_kid = t_gui_sk_node(t_tks, t_cur, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_ev_name, t_ev_fn, t_btns, (t_depth + 1));
ctron_list* t_fcl = (ctron_list*)(t_nfc);
ctron_list* t_nsl = (ctron_list*)(t_ns);
if ((t_prev < 0)) {
{
((ctron_list*)(t_fcl))->items[t_id] = (const char*)(long)(t_kid);
}
}
else {
{
((ctron_list*)(t_nsl))->items[t_prev] = (const char*)(long)(t_kid);
}
}
t_prev = t_kid;
}
}
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
return t_id;
}
int32_t t_gui_sk_style(ctron_list* t_tks, ctron_cell* t_cur, ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_ext) 
{
const char* t_name = t_gui_sk_word(t_tks, t_cur);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_ek = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strcmp((const char*)(t_ek), (const char*)("extends")) == 0)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
const char* t_pn = t_gui_sk_word(t_tks, t_cur);
ctron_list_push((ctron_list*)(t_ext), t_name);
ctron_list_push((ctron_list*)(t_ext), t_pn);
}
}
}
}
{
const char* t_pk = 0;
const char* t_p = 0;
const char* t_v = 0;
while ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
t_pk = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pk)) == 1)) {
{
int32_t t_pb = ctron_byte_at(t_pk, 0);
if ((t_pb == 59)) {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
continue;
}
}
if ((t_pb == 125)) {
{
break;
}
}
}
}
t_p = t_gui_sk_word(t_tks, t_cur);
t_gui_sk_word(t_tks, t_cur);
t_v = "";
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
const char* t_pv = ((const char*)((ctron_list*)(t_tks))->items[__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST)]);
if ((strlen((const char*)(t_pv)) > 1)) {
{
if ((ctron_byte_at(t_pv, 0) == 34)) {
{
t_v = t_gui_unq(t_pv);
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) + 1)), __ATOMIC_SEQ_CST);
}
}
}
}
}
}
if ((strcmp((const char*)(t_v), (const char*)("")) == 0)) {
{
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
t_v = t_gui_sk_word(t_tks, t_cur);
}
}
}
}
ctron_list_push((ctron_list*)(t_sk), ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_name), (const char*)("."))), (const char*)(t_p)));
ctron_list_push((ctron_list*)(t_sv), t_v);
}
}
return 0;
}
int32_t t_gui_sk_build(ctron_list* t_file, ctron_list* t_ntag, ctron_list* t_nflag, ctron_list* t_ncls, ctron_list* t_npre, ctron_list* t_nbid, ctron_list* t_npost, ctron_list* t_nfc, ctron_list* t_ns, ctron_list* t_nes, ctron_list* t_nec, ctron_list* t_ev_name, ctron_list* t_ev_fn, ctron_list* t_btns, ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_props) 
{
ctron_list* t_ext = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
ctron_list* t_tks = (ctron_list*)(ctron_list_new());
t_gui_split(((const char*)((ctron_list*)(t_d))->items[4]), t_tks);
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
if ((ctron_len((const void*)(t_d)) > 5)) {
{
t_gui_props_names(((const char*)((ctron_list*)(t_d))->items[5]), t_props);
}
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)(1), __ATOMIC_SEQ_CST);
if ((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) < ((ctron_list*)(t_tks))->n)) {
{
t_gui_sk_node(t_tks, t_cur, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_ev_name, t_ev_fn, t_btns, 1);
}
}
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("style")) == 0)) {
{
t_gui_sk_style(t_tks, t_cur, t_sk, t_sv, t_ext);
}
}
}
}
}
}
t_i += 1;
}
}
t_gui_sk_ext_resolve(t_sk, t_sv, t_ext);
return 0;
}
int32_t t_gui_sk_ext_resolve(ctron_list* t_sk, ctron_list* t_sv, ctron_list* t_ext) 
{
int32_t t_ci = 0;
{
const char* t_cur = 0;
int t_cyc = 0;
while ((t_ci < ((ctron_list*)(t_ext))->n)) {
t_cur = ((const char*)((ctron_list*)(t_ext))->items[t_ci]);
int32_t t_step = 0;
t_cyc = 1;
{
int t_found = 0;
while ((t_step <= ((ctron_list*)(t_ext))->n)) {
t_found = 0;
int32_t t_pi = 0;
while ((t_pi < ((ctron_list*)(t_ext))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ext))->items[t_pi])), (const char*)(t_cur)) == 0)) {
{
t_cur = ((const char*)((ctron_list*)(t_ext))->items[(t_pi + 1)]);
t_found = 1;
t_pi = ((ctron_list*)(t_ext))->n;
}
}
t_pi += 2;
}
if ((!t_found)) {
{
t_cyc = 0;
t_step = (((ctron_list*)(t_ext))->n + 1);
}
}
t_step += 1;
}
}
if (t_cyc) {
{
ctron_panic(ctron_str_concat((const char*)("gui: 样式 extends 环 "), (const char*)(((const char*)((ctron_list*)(t_ext))->items[t_ci]))));
}
}
t_ci += 2;
}
}
int t_changed = 1;
int32_t t_guard = 0;
while (t_changed) {
t_changed = 0;
t_guard += 1;
if ((t_guard > 16)) {
{
break;
}
}
int32_t t_pi = 0;
{
const char* t_child = 0;
const char* t_parent = 0;
const char* t_pp = 0;
while ((t_pi < ((ctron_list*)(t_ext))->n)) {
t_child = ((const char*)((ctron_list*)(t_ext))->items[t_pi]);
t_parent = ((const char*)((ctron_list*)(t_ext))->items[(t_pi + 1)]);
t_pp = ctron_str_concat((const char*)(t_parent), (const char*)("."));
int32_t t_si = 0;
{
const char* t_k = 0;
while ((t_si < ((ctron_list*)(t_sk))->n)) {
t_k = ((const char*)((ctron_list*)(t_sk))->items[t_si]);
if ((strlen((const char*)(t_k)) > strlen((const char*)(t_pp)))) {
{
if ((strcmp((const char*)(ctron_byte_slice(t_k, 0, strlen((const char*)(t_pp)))), (const char*)(t_pp)) == 0)) {
{
const char* t_pname = ctron_byte_slice(t_k, strlen((const char*)(t_pp)), strlen((const char*)(t_k)));
const char* t_ck = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_child), (const char*)("."))), (const char*)(t_pname));
if ((!t_gui_sk_has(t_sk, t_ck))) {
{
ctron_list_push((ctron_list*)(t_sk), t_ck);
ctron_list_push((ctron_list*)(t_sv), ((const char*)((ctron_list*)(t_sv))->items[t_si]));
t_changed = 1;
}
}
}
}
}
}
t_si += 1;
}
}
t_pi += 2;
}
}
}
return 0;
}
int t_gui_sk_has(ctron_list* t_sk, const char* t_key) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_sk))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sk))->items[t_i])), (const char*)(t_key)) == 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
int32_t t_gui_sk_dump_all(ctron_list* t_file) 
{
ctron_list* t_ntag = (ctron_list*)(ctron_list_new());
ctron_list* t_nflag = (ctron_list*)(ctron_list_new());
ctron_list* t_ncls = (ctron_list*)(ctron_list_new());
ctron_list* t_npre = (ctron_list*)(ctron_list_new());
ctron_list* t_nbid = (ctron_list*)(ctron_list_new());
ctron_list* t_npost = (ctron_list*)(ctron_list_new());
ctron_list* t_nfc = (ctron_list*)(ctron_list_new());
ctron_list* t_ns = (ctron_list*)(ctron_list_new());
ctron_list* t_nes = (ctron_list*)(ctron_list_new());
ctron_list* t_nec = (ctron_list*)(ctron_list_new());
ctron_list* t_ev_name = (ctron_list*)(ctron_list_new());
ctron_list* t_ev_fn = (ctron_list*)(ctron_list_new());
ctron_list* t_btns = (ctron_list*)(ctron_list_new());
ctron_list* t_sk = (ctron_list*)(ctron_list_new());
ctron_list* t_sv = (ctron_list*)(ctron_list_new());
ctron_list* t_props = (ctron_list*)(ctron_list_new());
t_gui_sk_build(t_file, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_ev_name, t_ev_fn, t_btns, t_sk, t_sv, t_props);
const char* t_pline = "";
int32_t t_pi = 0;
while ((t_pi < ((ctron_list*)(t_props))->n)) {
if ((t_pi > 0)) {
{
t_pline = ctron_str_concat((const char*)(t_pline), (const char*)(","));
}
}
t_pline = ctron_str_concat((const char*)(t_pline), (const char*)(((const char*)((ctron_list*)(t_props))->items[t_pi])));
t_pi += 1;
}
if ((strcmp((const char*)(t_pline), (const char*)("")) == 0)) {
{
t_pline = "-";
}
}
printf("%s\n", (const char*)(ctron_str_concat((const char*)("props="), (const char*)(t_pline))));
int32_t t_i = 0;
{
const char* t_pre = 0;
const char* t_bid = 0;
const char* t_post = 0;
const char* t_flag = 0;
const char* t_cls = 0;
while ((t_i < ((ctron_list*)(t_ntag))->n)) {
t_pre = ((const char*)((ctron_list*)(t_npre))->items[t_i]);
if ((strcmp((const char*)(t_pre), (const char*)("")) == 0)) {
{
t_pre = "-";
}
}
t_bid = ((const char*)((ctron_list*)(t_nbid))->items[t_i]);
if ((strcmp((const char*)(t_bid), (const char*)("")) == 0)) {
{
t_bid = "-";
}
}
t_post = ((const char*)((ctron_list*)(t_npost))->items[t_i]);
if ((strcmp((const char*)(t_post), (const char*)("")) == 0)) {
{
t_post = "-";
}
}
t_flag = ((const char*)((ctron_list*)(t_nflag))->items[t_i]);
if ((strcmp((const char*)(t_flag), (const char*)("")) == 0)) {
{
t_flag = "-";
}
}
t_cls = ((const char*)((ctron_list*)(t_ncls))->items[t_i]);
if ((strcmp((const char*)(t_cls), (const char*)("")) == 0)) {
{
t_cls = "-";
}
}
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("n"), (const char*)(ctron_i32_to_string((int32_t)(t_i))))), (const char*)(" tag="))), (const char*)(((const char*)((ctron_list*)(t_ntag))->items[t_i])))), (const char*)(" cls="))), (const char*)(t_cls))), (const char*)(" pre="))), (const char*)(t_pre))), (const char*)(" bid="))), (const char*)(t_bid))), (const char*)(" post="))), (const char*)(t_post))), (const char*)(" flag="))), (const char*)(t_flag))), (const char*)(" fc="))), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_nfc))->items[t_i])))))), (const char*)(" ns="))), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_ns))->items[t_i])))))), (const char*)(" es="))), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_nes))->items[t_i])))))), (const char*)(" ec="))), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_nec))->items[t_i])))))));
t_i += 1;
}
}
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_ev_name))->n)) {
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("e"), (const char*)(ctron_i32_to_string((int32_t)(t_k))))), (const char*)(" on="))), (const char*)(((const char*)((ctron_list*)(t_ev_name))->items[t_k])))), (const char*)(" fn="))), (const char*)(((const char*)((ctron_list*)(t_ev_fn))->items[t_k])))));
t_k += 1;
}
int32_t t_b = 0;
while ((t_b < ((ctron_list*)(t_btns))->n)) {
printf("%s\n", (const char*)(ctron_str_concat((const char*)("btn id="), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_btns))->items[t_b])))))));
t_b += 1;
}
int32_t t_s = 0;
while ((t_s < ((ctron_list*)(t_sk))->n)) {
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("s "), (const char*)(((const char*)((ctron_list*)(t_sk))->items[t_s])))), (const char*)("="))), (const char*)(((const char*)((ctron_list*)(t_sv))->items[t_s])))));
t_s += 1;
}
printf("%s\n", (const char*)("sk-end"));
return 0;
}
const char* t_gui_sk_cesc(const char* t_s) 
{
const char* t_out = "";
int32_t t_i = 0;
int32_t t_start = 0;
{
int32_t t_b = 0;
while ((t_i < strlen((const char*)(t_s)))) {
t_b = ctron_byte_at(t_s, t_i);
if ((t_b == 34)) {
{
if ((t_i > t_start)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_start, t_i)));
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\\"));
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\""));
t_i += 1;
t_start = t_i;
}
}
else {
{
if ((t_b == 92)) {
{
if ((t_i > t_start)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_start, t_i)));
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\\"));
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\\"));
t_i += 1;
t_start = t_i;
}
}
else {
{
if ((t_b == 10)) {
{
if ((t_i > t_start)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_start, t_i)));
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\\"));
t_out = ctron_str_concat((const char*)(t_out), (const char*)("n"));
t_i += 1;
t_start = t_i;
}
}
else {
{
t_i += 1;
}
}
}
}
}
}
}
}
if ((strlen((const char*)(t_s)) > t_start)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_start, strlen((const char*)(t_s)))));
}
}
return t_out;
}
const char* t_gui_sk_emit_str_arr(int32_t t_id, ctron_list* t_tbl) 
{
const char* t_arr = ctron_str_concat((const char*)(ctron_str_concat((const char*)("static const char* sk_t"), (const char*)(ctron_i32_to_string((int32_t)(t_id))))), (const char*)("[] = {"));
int32_t t_k2 = 0;
while ((t_k2 < ((ctron_list*)(t_tbl))->n)) {
if ((t_k2 > 0)) {
{
t_arr = ctron_str_concat((const char*)(t_arr), (const char*)(","));
}
}
t_arr = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_arr), (const char*)(" \""))), (const char*)(t_gui_sk_cesc(((const char*)((ctron_list*)(t_tbl))->items[t_k2]))))), (const char*)("\""));
t_k2 += 1;
}
return ctron_str_concat((const char*)(t_arr), (const char*)(" };"));
}
const char* t_gui_sk_emit_int_arr(int32_t t_id, ctron_list* t_tbl) 
{
const char* t_arr = ctron_str_concat((const char*)(ctron_str_concat((const char*)("static const int32_t sk_t"), (const char*)(ctron_i32_to_string((int32_t)(t_id))))), (const char*)("[] = {"));
int32_t t_k2 = 0;
while ((t_k2 < ((ctron_list*)(t_tbl))->n)) {
if ((t_k2 > 0)) {
{
t_arr = ctron_str_concat((const char*)(t_arr), (const char*)(","));
}
}
t_arr = ctron_str_concat((const char*)(t_arr), (const char*)(ctron_i32_to_string((int32_t)(((int32_t)(long)((ctron_list*)(t_tbl))->items[t_k2])))));
t_k2 += 1;
}
return ctron_str_concat((const char*)(t_arr), (const char*)(" };"));
}
ctron_list* t_gui_sk_wrap_s(ctron_list* t_src) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_out), "L");
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_src))->n)) {
ctron_list_push((ctron_list*)(t_out), (char*)(t_v_box(t_vS(((const char*)((ctron_list*)(t_src))->items[t_i])))));
t_i += 1;
}
return t_out;
}
ctron_list* t_gui_sk_wrap_i(ctron_list* t_src) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_out), "L");
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_src))->n)) {
ctron_list_push((ctron_list*)(t_out), (char*)(t_v_box(t_vI(((int32_t)(long)((ctron_list*)(t_src))->items[t_i])))));
t_i += 1;
}
return t_out;
}
ctron_list* t_gui_ds_ptyps(ctron_list* t_file, ctron_list* t_pnames) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
ctron_list* t_vn = (ctron_list*)(ctron_list_new());
ctron_list* t_vt = (ctron_list*)(ctron_list_new());
if ((ctron_len((const void*)(t_d)) > 5)) {
{
t_gui_props_typed(((const char*)((ctron_list*)(t_d))->items[5]), t_vn, t_vt);
}
}
if ((((ctron_list*)(t_vn))->n == ((ctron_list*)(t_pnames))->n)) {
{
int t_same = 1;
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_vn))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vn))->items[t_k])), (const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_k]))) != 0)) {
{
t_same = 0;
t_k = ((ctron_list*)(t_vn))->n;
}
}
t_k += 1;
}
if (t_same) {
{
int32_t t_w = 0;
while ((t_w < ((ctron_list*)(t_vt))->n)) {
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_vt))->items[t_w]));
t_w += 1;
}
return t_out;
}
}
}
}
}
}
}
}
t_i += 1;
}
}
return t_out;
}
int32_t t_gui_ds_collect_slots(ctron_list* t_file, ctron_list* t_pnames, ctron_list* t_paths, ctron_list* t_pty, ctron_list* t_lists, ctron_list* t_evn, ctron_list* t_evf, ctron_list* t_inps, ctron_list* t_ievs, ctron_list* t_crows, ctron_list* t_cty, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_cvn, ctron_list* t_cvf, ctron_list* t_ciev) 
{
ctron_list* t_ntag = (ctron_list*)(ctron_list_new());
ctron_list* t_nflag = (ctron_list*)(ctron_list_new());
ctron_list* t_ncls = (ctron_list*)(ctron_list_new());
ctron_list* t_npre = (ctron_list*)(ctron_list_new());
ctron_list* t_nbid = (ctron_list*)(ctron_list_new());
ctron_list* t_npost = (ctron_list*)(ctron_list_new());
ctron_list* t_nfc = (ctron_list*)(ctron_list_new());
ctron_list* t_ns = (ctron_list*)(ctron_list_new());
ctron_list* t_nes = (ctron_list*)(ctron_list_new());
ctron_list* t_nec = (ctron_list*)(ctron_list_new());
ctron_list* t_f_evn = (ctron_list*)(ctron_list_new());
ctron_list* t_f_evf = (ctron_list*)(ctron_list_new());
ctron_list* t_btns = (ctron_list*)(ctron_list_new());
ctron_list* t_sk = (ctron_list*)(ctron_list_new());
ctron_list* t_sv = (ctron_list*)(ctron_list_new());
ctron_list* t_skprops = (ctron_list*)(ctron_list_new());
t_gui_sk_build(t_file, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_f_evn, t_f_evf, t_btns, t_sk, t_sv, t_skprops);
ctron_list* t_ptyps = (ctron_list*)(t_gui_ds_ptyps(t_file, t_pnames));
ctron_list* t_isch = (ctron_list*)(ctron_list_new());
int32_t t_z = 0;
while ((t_z < ((ctron_list*)(t_ntag))->n)) {
ctron_list_push((ctron_list*)(t_isch), (const char*)(long)(0));
t_z += 1;
}
int32_t t_z2 = 0;
while ((t_z2 < ((ctron_list*)(t_ntag))->n)) {
int32_t t_ch2 = ((int32_t)(long)((ctron_list*)(t_nfc))->items[t_z2]);
while ((t_ch2 >= 0)) {
((ctron_list*)(t_isch))->items[t_ch2] = (const char*)(long)(1);
t_ch2 = ((int32_t)(long)((ctron_list*)(t_ns))->items[t_ch2]);
}
t_z2 += 1;
}
ctron_list* t_exprs = (ctron_list*)(ctron_list_new());
ctron_list* t_rst = (ctron_list*)(ctron_list_new());
int32_t t_z3 = 0;
while ((t_z3 < ((ctron_list*)(t_ntag))->n)) {
if ((((int32_t)(long)((ctron_list*)(t_isch))->items[t_z3]) == 0)) {
{
ctron_list_push((ctron_list*)(t_rst), (const char*)(long)(t_z3));
}
}
t_z3 += 1;
}
int32_t t_ri4 = 0;
{
int t_istgt = 0;
while ((t_ri4 < ((ctron_list*)(t_rst))->n)) {
t_istgt = 0;
if ((t_ri4 == (((ctron_list*)(t_rst))->n - 1))) {
{
t_istgt = 1;
}
}
ctron_list* t_stid = (ctron_list*)(ctron_list_new());
ctron_list* t_stie = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_stid), (const char*)(long)(((int32_t)(long)((ctron_list*)(t_rst))->items[t_ri4])));
ctron_list_push((ctron_list*)(t_stie), (const char*)(long)(0));
int32_t t_si = 0;
{
int32_t t_id = 0;
int32_t t_ie = 0;
while ((t_si < ((ctron_list*)(t_stid))->n)) {
t_id = ((int32_t)(long)((ctron_list*)(t_stid))->items[t_si]);
t_ie = ((int32_t)(long)((ctron_list*)(t_stie))->items[t_si]);
t_si += 1;
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ntag))->items[t_id])), (const char*)("each")) == 0)) {
{
t_ie = 1;
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_lists), ((const char*)((ctron_list*)(t_npre))->items[t_id]));
}
}
}
}
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_ntag))->items[t_id]))) > 5)) {
{
if ((strcmp((const char*)(ctron_byte_slice(((const char*)((ctron_list*)(t_ntag))->items[t_id]), 0, 5)), (const char*)("call:")) == 0)) {
{
const char* t_atx = ((const char*)((ctron_list*)(t_npost))->items[t_id]);
int32_t t_ai = 0;
int32_t t_aos = 0;
while ((t_ai <= strlen((const char*)(t_atx)))) {
if ((t_ai == strlen((const char*)(t_atx)))) {
{
if ((t_ai > t_aos)) {
{
ctron_list_push((ctron_list*)(t_exprs), ctron_byte_slice(t_atx, t_aos, t_ai));
}
}
t_ai += 1;
}
}
else {
{
if ((ctron_byte_at(t_atx, t_ai) == 1)) {
{
if ((t_ai > t_aos)) {
{
ctron_list_push((ctron_list*)(t_exprs), ctron_byte_slice(t_atx, t_aos, t_ai));
}
}
t_aos = (t_ai + 1);
}
}
t_ai += 1;
}
}
}
}
}
}
}
if (t_tag_is_input_evt(((const char*)((ctron_list*)(t_ntag))->items[t_id]), ((const char*)((ctron_list*)(t_nbid))->items[t_id]))) {
{
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_crows), ((const char*)((ctron_list*)(t_nbid))->items[t_id]));
ctron_list_push((ctron_list*)(t_cty), "Bool");
}
}
}
}
else {
{
if (t_istgt) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_nbid))->items[t_id])), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_exprs), ((const char*)((ctron_list*)(t_nbid))->items[t_id]));
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_nflag))->items[t_id])), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_exprs), ((const char*)((ctron_list*)(t_nflag))->items[t_id]));
}
}
}
}
}
}
int32_t t_e = 0;
{
int32_t t_gi = 0;
while ((t_e < ((int32_t)(long)((ctron_list*)(t_nec))->items[t_id]))) {
t_gi = (((int32_t)(long)((ctron_list*)(t_nes))->items[t_id]) + t_e);
if ((t_ie == 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_f_evn))->items[t_gi])), (const char*)("input")) == 0)) {
{
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_inps), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
}
}
else {
{
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_ievs), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_ciev), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_f_evn))->items[t_gi])), (const char*)("input")) == 0)) {
{
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_inps), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
}
}
else {
{
if (t_istgt) {
{
ctron_list_push((ctron_list*)(t_evn), ((const char*)((ctron_list*)(t_f_evn))->items[t_gi]));
ctron_list_push((ctron_list*)(t_evf), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_cvn), ((const char*)((ctron_list*)(t_f_evn))->items[t_gi]));
ctron_list_push((ctron_list*)(t_cvf), ((const char*)((ctron_list*)(t_f_evf))->items[t_gi]));
}
}
}
}
}
}
t_e += 1;
}
}
int32_t t_ch = ((int32_t)(long)((ctron_list*)(t_nfc))->items[t_id]);
while ((t_ch >= 0)) {
ctron_list_push((ctron_list*)(t_stid), (const char*)(long)(t_ch));
ctron_list_push((ctron_list*)(t_stie), (const char*)(long)(t_ie));
t_ch = ((int32_t)(long)((ctron_list*)(t_ns))->items[t_ch]);
}
}
}
t_ri4 += 1;
}
}
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_exprs))->n)) {
ctron_list* t_ps = (ctron_list*)(ctron_list_new());
t_gui_bx_dotted(((const char*)((ctron_list*)(t_exprs))->items[t_i]), t_ps);
int32_t t_j = 0;
{
const char* t_rt = 0;
while ((t_j < ((ctron_list*)(t_ps))->n)) {
t_rt = ((const char*)((ctron_list*)(t_ps))->items[t_j]);
int32_t t_di = 0;
while ((t_di < ctron_len((const void*)(((const char*)((ctron_list*)(t_ps))->items[t_j]))))) {
if ((ctron_byte_at(((const char*)((ctron_list*)(t_ps))->items[t_j]), t_di) == 46)) {
{
t_rt = ctron_byte_slice(((const char*)((ctron_list*)(t_ps))->items[t_j]), 0, t_di);
t_di = ctron_len((const void*)(((const char*)((ctron_list*)(t_ps))->items[t_j])));
}
}
t_di += 1;
}
if (t_gui_ck_has_style(t_pnames, t_rt)) {
{
if ((!t_gui_ck_has_style(t_paths, ((const char*)((ctron_list*)(t_ps))->items[t_j])))) {
{
ctron_list_push((ctron_list*)(t_paths), ((const char*)((ctron_list*)(t_ps))->items[t_j]));
ctron_list_push((ctron_list*)(t_pty), t_gui_ds_path_ty(((const char*)((ctron_list*)(t_ps))->items[t_j]), t_pnames, t_ptyps, t_stns, t_sfk, t_stv));
}
}
}
}
t_j += 1;
}
}
t_i += 1;
}
return 0;
}
int t_tag_is_input_evt(const char* t_tag, const char* t_nbid) 
{
if ((strcmp((const char*)(t_tag), (const char*)("checkbox")) == 0)) {
{
if ((strcmp((const char*)(t_nbid), (const char*)("")) != 0)) {
{
return 1;
}
}
}
}
return 0;
}
ctron_list* t_gui_ds_gen(ctron_list* t_file, const char* t_vname, ctron_list* t_pnames, ctron_list* t_ptyps, ctron_list* t_ordn, ctron_list* t_paths, ctron_list* t_pty, ctron_list* t_lists, ctron_list* t_evn, ctron_list* t_evf, ctron_list* t_inps, ctron_list* t_ievs, ctron_list* t_crows, ctron_list* t_cty, ctron_list* t_stns, ctron_list* t_sfk, ctron_list* t_stv, ctron_list* t_cvn, ctron_list* t_cvf, ctron_list* t_ciev) 
{
ctron_list* t_ptoks = (ctron_list*)(ctron_list_new());
ctron_list* t_boxes = (ctron_list*)(ctron_list_new());
int32_t t_ii = 0;
{
const char* t_pn = 0;
const char* t_ty = 0;
const char* t_head = 0;
const char* t_tok = 0;
while ((t_ii < ((ctron_list*)(t_ordn))->n)) {
t_pn = ((const char*)((ctron_list*)(t_ordn))->items[t_ii]);
t_ty = "";
int32_t t_ti = 0;
while ((t_ti < ((ctron_list*)(t_pnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_ti])), (const char*)(t_pn)) == 0)) {
{
t_ty = ((const char*)((ctron_list*)(t_ptyps))->items[t_ti]);
t_ti = ((ctron_list*)(t_pnames))->n;
}
}
t_ti += 1;
}
t_head = t_gui_ty_head_txt(t_ty);
t_tok = ctron_str_concat((const char*)("__a"), (const char*)(ctron_i32_to_string((int32_t)(t_ii))));
if (t_gui_ck_has_style(t_stns, t_head)) {
{
t_tok = ctron_str_concat((const char*)("__p_"), (const char*)(t_pn));
const char* t_bl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("    var "), (const char*)(t_tok))), (const char*)(": Box["))), (const char*)(t_head))), (const char*)("] = "));
if ((strcmp((const char*)(t_head), (const char*)(t_gui_trim(t_ty))) == 0)) {
{
t_bl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_bl), (const char*)("Box["))), (const char*)(t_head))), (const char*)("]("))), (const char*)("__a"))), (const char*)(ctron_i32_to_string((int32_t)(t_ii))))), (const char*)(")"));
}
}
else {
{
t_bl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_bl), (const char*)("__a"))), (const char*)(ctron_i32_to_string((int32_t)(t_ii))));
}
}
ctron_list_push((ctron_list*)(t_boxes), t_bl);
}
}
ctron_list_push((ctron_list*)(t_ptoks), t_tok);
t_ii += 1;
}
}
const char* t_params = "";
const char* t_cargs = "";
const char* t_sigargs = "";
t_ii = 0;
{
const char* t_pn = 0;
const char* t_ty = 0;
const char* t_head2 = 0;
while ((t_ii < ((ctron_list*)(t_ordn))->n)) {
if ((t_ii > 0)) {
{
t_params = ctron_str_concat((const char*)(t_params), (const char*)(", "));
t_cargs = ctron_str_concat((const char*)(t_cargs), (const char*)(", "));
t_sigargs = ctron_str_concat((const char*)(t_sigargs), (const char*)(", "));
}
}
t_pn = ((const char*)((ctron_list*)(t_ordn))->items[t_ii]);
t_ty = "";
int32_t t_ti = 0;
while ((t_ti < ((ctron_list*)(t_pnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pnames))->items[t_ti])), (const char*)(t_pn)) == 0)) {
{
t_ty = t_gui_trim(((const char*)((ctron_list*)(t_ptyps))->items[t_ti]));
t_ti = ((ctron_list*)(t_pnames))->n;
}
}
t_ti += 1;
}
t_params = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_params), (const char*)("__a"))), (const char*)(ctron_i32_to_string((int32_t)(t_ii))))), (const char*)(": "))), (const char*)(t_ty));
t_cargs = ctron_str_concat((const char*)(t_cargs), (const char*)(((const char*)((ctron_list*)(t_ptoks))->items[t_ii])));
t_head2 = t_gui_ty_head_txt(t_ty);
if (t_gui_ck_has_style(t_stns, t_head2)) {
{
t_sigargs = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_sigargs), (const char*)(((const char*)((ctron_list*)(t_ptoks))->items[t_ii])))), (const char*)(": Box["))), (const char*)(t_head2))), (const char*)("]"));
}
}
else {
{
t_sigargs = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_sigargs), (const char*)(((const char*)((ctron_list*)(t_ptoks))->items[t_ii])))), (const char*)(": "))), (const char*)(t_ty));
}
}
t_ii += 1;
}
}
const char* t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)("fn __gui_bind_"), (const char*)(t_vname))), (const char*)("(buf: List[Str]"));
if ((strlen((const char*)(t_sigargs)) > 0)) {
{
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)(", "))), (const char*)(t_sigargs));
}
}
t_g = ctron_str_concat((const char*)(t_g), (const char*)(") {\n"));
int32_t t_k = 0;
{
const char* t_rw = 0;
const char* t_tyk = 0;
while ((t_k < ((ctron_list*)(t_paths))->n)) {
t_rw = t_gui_ds_rw(((const char*)((ctron_list*)(t_paths))->items[t_k]), t_pnames, t_ptoks);
t_tyk = ((const char*)((ctron_list*)(t_pty))->items[t_k]);
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("    if buf[0] == \"prop:"))), (const char*)(((const char*)((ctron_list*)(t_paths))->items[t_k])))), (const char*)("\" {\n"));
if ((strcmp((const char*)(t_tyk), (const char*)("I32")) == 0)) {
{
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("        buf.push(\"i:\" + ("))), (const char*)(t_rw))), (const char*)(").to_string())\n"));
}
}
else {
{
if ((strcmp((const char*)(t_tyk), (const char*)("Bool")) == 0)) {
{
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("        if ("))), (const char*)(t_rw))), (const char*)(") { buf.push(\"b:1\") } else { buf.push(\"b:0\") }\n"));
}
}
else {
{
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("        buf.push("))), (const char*)(t_rw))), (const char*)(")\n"));
}
}
}
}
t_g = ctron_str_concat((const char*)(t_g), (const char*)("    }\n"));
t_k += 1;
}
}
t_k = 0;
{
const char* t_rwc = 0;
int32_t t_plc = 0;
while ((t_k < ((ctron_list*)(t_crows))->n)) {
t_rwc = t_gui_ds_rw(((const char*)((ctron_list*)(t_crows))->items[t_k]), t_pnames, t_ptoks);
t_plc = (ctron_len((const void*)(((const char*)((ctron_list*)(t_crows))->items[t_k]))) + 9);
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("    if buf[0].len >= "))), (const char*)(ctron_i32_to_string((int32_t)(t_plc))))), (const char*)(" {\n"));
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("        if byte_slice(buf[0], 0, "))), (const char*)(ctron_i32_to_string((int32_t)(t_plc))))), (const char*)(") == \"eachrow:"))), (const char*)(((const char*)((ctron_list*)(t_crows))->items[t_k])))), (const char*)(":\" {\n"));
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("            if "))), (const char*)(t_rwc))), (const char*)("[ev_suffix_i(buf[0])] { buf.push(\"1\") } else { buf.push(\"0\") }\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("        }\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("    }\n"));
t_k += 1;
}
}
t_k = 0;
{
const char* t_rwl = 0;
while ((t_k < ((ctron_list*)(t_lists))->n)) {
t_rwl = t_gui_ds_rw(((const char*)((ctron_list*)(t_lists))->items[t_k]), t_pnames, t_ptoks);
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("    if buf[0] == \"each:"))), (const char*)(((const char*)((ctron_list*)(t_lists))->items[t_k])))), (const char*)("\" {\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("        var gi: I32 = 0\n"));
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("        while gi < "))), (const char*)(t_rwl))), (const char*)(".len {\n"));
t_g = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)("            buf.push("))), (const char*)(t_rwl))), (const char*)("[gi])\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("            gi += 1\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("        }\n"));
t_g = ctron_str_concat((const char*)(t_g), (const char*)("    }\n"));
t_k += 1;
}
}
t_g = ctron_str_concat((const char*)(t_g), (const char*)("}\n"));
const char* t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)("fn __gui_act_"), (const char*)(t_vname))), (const char*)("(name: Str, args: List[Str]"));
if ((strlen((const char*)(t_sigargs)) > 0)) {
{
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)(", "))), (const char*)(t_sigargs));
}
}
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)(") {\n"));
const char* t_mcap = "";
if ((((ctron_list*)(t_ptoks))->n > 0)) {
{
t_mcap = ((const char*)((ctron_list*)(t_ptoks))->items[0]);
}
}
t_k = 0;
{
const char* t_headC = 0;
while ((t_k < ((ctron_list*)(t_cvf))->n)) {
ctron_list* t_argcC = (ctron_list*)(ctron_list_new());
ctron_list* t_aoC = (ctron_list*)(ctron_list_new());
t_headC = t_gui_ev_head2(((const char*)((ctron_list*)(t_cvf))->items[t_k]), t_argcC, t_aoC);
if ((strlen((const char*)(t_headC)) > 0)) {
{
ctron_list* t_ptsl = (ctron_list*)(t_gui_fn_ptyps(t_file, t_headC));
int t_hasm = 0;
if ((((ctron_list*)(t_ptsl))->n > 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_ptsl))->items[0]))) >= 4)) {
{
if ((strcmp((const char*)(ctron_byte_slice(((const char*)((ctron_list*)(t_ptsl))->items[0]), 0, 4)), (const char*)("Box:")) == 0)) {
{
t_hasm = 1;
}
}
}
}
}
}
if ((((int32_t)(long)((ctron_list*)(t_argcC))->items[0]) >= 0)) {
{
const char* t_parr = "";
if (t_hasm) {
{
t_parr = ctron_str_concat((const char*)(t_mcap), (const char*)(", "));
}
}
int32_t t_pi2 = 0;
{
int32_t t_didx = 0;
const char* t_dc = 0;
while ((t_pi2 < ((int32_t)(long)((ctron_list*)(t_argcC))->items[0]))) {
t_didx = t_pi2;
if (t_hasm) {
{
t_didx = (t_pi2 + 1);
}
}
t_dc = "ev_arg_s";
if ((t_didx < ((ctron_list*)(t_ptsl))->n)) {
{
const char* t_pts = ((const char*)((ctron_list*)(t_ptsl))->items[t_didx]);
if ((strcmp((const char*)(t_pts), (const char*)("i")) == 0)) {
{
t_dc = "ev_arg_i";
}
}
else {
{
if ((strcmp((const char*)(t_pts), (const char*)("b")) == 0)) {
{
t_dc = "ev_arg_b";
}
}
}
}
}
}
if ((t_pi2 > 0)) {
{
t_parr = ctron_str_concat((const char*)(t_parr), (const char*)(", "));
}
}
t_parr = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_parr), (const char*)(t_dc))), (const char*)("(args, "))), (const char*)(ctron_i32_to_string((int32_t)(t_pi2))))), (const char*)(")"));
t_pi2 += 1;
}
}
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(t_headC))), (const char*)("\" { "))), (const char*)(t_headC))), (const char*)("("))), (const char*)(t_parr))), (const char*)(") }\n"));
}
}
else {
{
if (t_hasm) {
{
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(t_headC))), (const char*)("\" { "))), (const char*)(t_headC))), (const char*)("("))), (const char*)(t_mcap))), (const char*)(") }\n"));
}
}
else {
{
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(t_headC))), (const char*)("\" { "))), (const char*)(t_headC))), (const char*)("() }\n"));
}
}
}
}
}
}
t_k += 1;
}
}
t_k = 0;
{
const char* t_headC2 = 0;
while ((t_k < ((ctron_list*)(t_ciev))->n)) {
ctron_list* t_argcC2 = (ctron_list*)(ctron_list_new());
ctron_list* t_aoC2 = (ctron_list*)(ctron_list_new());
t_headC2 = t_gui_ev_head2(((const char*)((ctron_list*)(t_ciev))->items[t_k]), t_argcC2, t_aoC2);
if ((strlen((const char*)(t_headC2)) > 0)) {
{
int32_t t_hl = strlen((const char*)(t_headC2));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name.len > "))), (const char*)(ctron_i32_to_string((int32_t)(t_hl))))), (const char*)(" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("        if byte_slice(name, 0, "))), (const char*)(ctron_i32_to_string((int32_t)(t_hl))))), (const char*)(") == \""))), (const char*)(t_headC2))), (const char*)("\" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("        if byte_at(name, "))), (const char*)(ctron_i32_to_string((int32_t)(t_hl))))), (const char*)(") == 58 {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("            "))), (const char*)(t_headC2))), (const char*)("("))), (const char*)(t_mcap))), (const char*)(", ev_suffix_i(name))\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("        }\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("        }\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("    }\n"));
}
}
t_k += 1;
}
}
t_k = 0;
{
const char* t_head2 = 0;
while ((t_k < ((ctron_list*)(t_evf))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_evn))->items[t_k])), (const char*)("input")) == 0)) {
{
if (t_gui_ck_has_style(t_inps, ((const char*)((ctron_list*)(t_evf))->items[t_k]))) {
{
t_k += 1;
continue;
}
}
}
}
if (t_gui_ck_has_style(t_ievs, ((const char*)((ctron_list*)(t_evf))->items[t_k]))) {
{
t_k += 1;
continue;
}
}
ctron_list* t_argc = (ctron_list*)(ctron_list_new());
ctron_list* t_ao = (ctron_list*)(ctron_list_new());
t_head2 = t_gui_ev_head2(((const char*)((ctron_list*)(t_evf))->items[t_k]), t_argc, t_ao);
if ((strlen((const char*)(t_head2)) > 0)) {
{
if ((((ctron_list*)(t_argc))->n > 0)) {
{
if ((((int32_t)(long)((ctron_list*)(t_argc))->items[0]) >= 1)) {
{
const char* t_sp = ((const char*)((ctron_list*)(t_ao))->items[0]);
ctron_list* t_parts = (ctron_list*)(ctron_list_new());
int32_t t_si = 0;
int32_t t_depth = 0;
const char* t_cur0 = "";
{
int32_t t_c = 0;
while ((t_si < strlen((const char*)(t_sp)))) {
t_c = ctron_byte_at(t_sp, t_si);
if ((t_c == 40)) {
{
t_depth += 1;
}
}
else {
{
if ((t_c == 41)) {
{
t_depth -= 1;
}
}
else {
{
if ((t_c == 44)) {
{
if ((t_depth == 0)) {
{
ctron_list_push((ctron_list*)(t_parts), t_gui_trim(t_cur0));
t_cur0 = "";
t_si += 1;
continue;
}
}
}
}
}
}
}
}
t_cur0 = ctron_str_concat((const char*)(t_cur0), (const char*)(ctron_byte_slice(t_sp, t_si, (t_si + 1))));
t_si += 1;
}
}
ctron_list_push((ctron_list*)(t_parts), t_gui_trim(t_cur0));
const char* t_ar = "";
int32_t t_pj = 0;
while ((t_pj < ((ctron_list*)(t_parts))->n)) {
if ((t_pj > 0)) {
{
t_ar = ctron_str_concat((const char*)(t_ar), (const char*)(", "));
}
}
t_ar = ctron_str_concat((const char*)(t_ar), (const char*)(t_gui_ds_rw(((const char*)((ctron_list*)(t_parts))->items[t_pj]), t_pnames, t_ptoks)));
t_pj += 1;
}
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(t_head2))), (const char*)("\" { "))), (const char*)(t_head2))), (const char*)("("))), (const char*)(t_ar))), (const char*)(") }\n"));
}
}
else {
{
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(t_head2))), (const char*)("\" { "))), (const char*)(t_head2))), (const char*)("() }\n"));
}
}
}
}
else {
{
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name == \""))), (const char*)(((const char*)((ctron_list*)(t_evf))->items[t_k])))), (const char*)("\" { "))), (const char*)(((const char*)((ctron_list*)(t_evf))->items[t_k])))), (const char*)("() }\n"));
}
}
}
}
t_k += 1;
}
}
t_k = 0;
{
const char* t_headE = 0;
while ((t_k < ((ctron_list*)(t_ievs))->n)) {
ctron_list* t_argcE = (ctron_list*)(ctron_list_new());
ctron_list* t_aoE = (ctron_list*)(ctron_list_new());
t_headE = t_gui_ev_head2(((const char*)((ctron_list*)(t_ievs))->items[t_k]), t_argcE, t_aoE);
if ((strlen((const char*)(t_headE)) > 0)) {
{
const char* t_arE = "";
if ((((ctron_list*)(t_argcE))->n > 0)) {
{
if ((((int32_t)(long)((ctron_list*)(t_argcE))->items[0]) >= 1)) {
{
t_arE = t_gui_ds_rw(((const char*)((ctron_list*)(t_aoE))->items[0]), t_pnames, t_ptoks);
}
}
}
}
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name.len > "))), (const char*)(ctron_i32_to_string((int32_t)(strlen((const char*)(t_headE))))))), (const char*)(" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("        if byte_slice(name, 0, "))), (const char*)(ctron_i32_to_string((int32_t)(strlen((const char*)(t_headE))))))), (const char*)(") == \""))), (const char*)(t_headE))), (const char*)("\" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("        if byte_at(name, "))), (const char*)(ctron_i32_to_string((int32_t)(strlen((const char*)(t_headE))))))), (const char*)(") == 58 {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("            "))), (const char*)(t_headE))), (const char*)("("))), (const char*)(t_arE))), (const char*)(", ev_suffix_i(name))\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("        }\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("        }\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("    }\n"));
}
}
t_k += 1;
}
}
t_k = 0;
{
const char* t_headI = 0;
while ((t_k < ((ctron_list*)(t_inps))->n)) {
ctron_list* t_argcI = (ctron_list*)(ctron_list_new());
ctron_list* t_aoI = (ctron_list*)(ctron_list_new());
t_headI = t_gui_ev_head2(((const char*)((ctron_list*)(t_inps))->items[t_k]), t_argcI, t_aoI);
if ((strlen((const char*)(t_headI)) > 0)) {
{
if ((((ctron_list*)(t_argcI))->n == 0)) {
{
t_k = ((ctron_list*)(t_inps))->n;
continue;
}
}
int32_t t_pl = (strlen((const char*)(t_headI)) + 1);
const char* t_arI = "";
if ((((ctron_list*)(t_argcI))->n > 0)) {
{
if ((((int32_t)(long)((ctron_list*)(t_argcI))->items[0]) >= 1)) {
{
t_arI = t_gui_ds_rw(((const char*)((ctron_list*)(t_aoI))->items[0]), t_pnames, t_ptoks);
}
}
}
}
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("    if name.len >= "))), (const char*)(ctron_i32_to_string((int32_t)(t_pl))))), (const char*)(" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("        if byte_slice(name, 0, "))), (const char*)(ctron_i32_to_string((int32_t)(t_pl))))), (const char*)(") == \""))), (const char*)(t_headI))), (const char*)(":\" {\n"));
t_a2 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_a2), (const char*)("            "))), (const char*)(t_headI))), (const char*)("("))), (const char*)(t_arI))), (const char*)(", byte_slice(name, "))), (const char*)(ctron_i32_to_string((int32_t)(t_pl))))), (const char*)(", name.len))\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("        }\n"));
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("    }\n"));
}
}
t_k += 1;
}
}
t_a2 = ctron_str_concat((const char*)(t_a2), (const char*)("}\n"));
const char* t_r = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("fn __gui_run_"), (const char*)(t_vname))), (const char*)("("))), (const char*)(t_params))), (const char*)(") -> I32 {\n"));
t_k = 0;
while ((t_k < ((ctron_list*)(t_boxes))->n)) {
t_r = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_r), (const char*)(((const char*)((ctron_list*)(t_boxes))->items[t_k])))), (const char*)("\n"));
t_k += 1;
}
const char* t_bindcl = ctron_str_concat((const char*)(ctron_str_concat((const char*)("|buf| __gui_bind_"), (const char*)(t_vname))), (const char*)("(buf"));
if ((strlen((const char*)(t_cargs)) > 0)) {
{
t_bindcl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_bindcl), (const char*)(", "))), (const char*)(t_cargs));
}
}
t_bindcl = ctron_str_concat((const char*)(t_bindcl), (const char*)(")"));
const char* t_actcl = ctron_str_concat((const char*)(ctron_str_concat((const char*)("|nm, ar| __gui_act_"), (const char*)(t_vname))), (const char*)("(nm, ar"));
if ((strlen((const char*)(t_cargs)) > 0)) {
{
t_actcl = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_actcl), (const char*)(", "))), (const char*)(t_cargs));
}
}
t_actcl = ctron_str_concat((const char*)(t_actcl), (const char*)(")"));
t_r = ctron_str_concat((const char*)(t_r), (const char*)("    return rt_run_anchor(\"Ctron\", 320, 480,\n"));
t_r = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_r), (const char*)("        "))), (const char*)(t_bindcl))), (const char*)(",\n"));
t_r = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_r), (const char*)("        "))), (const char*)(t_actcl))), (const char*)(")\n"));
t_r = ctron_str_concat((const char*)(t_r), (const char*)("}\n"));
const char* t_src = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_g), (const char*)(t_a2))), (const char*)(t_r));
(ctron_fs_write("/tmp/ds_gen_dump.ct", t_src)?1:0);
ctron_list* t_toks = (ctron_list*)(ctron_list_new());
ctron_list* t_lns = (ctron_list*)(ctron_list_new());
ctron_list* t_cols = (ctron_list*)(ctron_list_new());
ctron_list* t_pd = (ctron_list*)(ctron_list_new());
t_scan4(t_src, t_toks, t_lns, t_cols, t_pd);
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
return t_p_file(t_toks, t_cur, t_lns, t_cols, t_pd);
}
const char* t_gui_ds_marker_expr(ctron_list* t_e) 
{
if ((((ctron_list*)(t_e))->n < 1)) {
{
return "";
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1])), (const char*)("run")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 2)) {
{
const char* t_a = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a))->items[0])), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))) > 1)) {
{
return ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1]);
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
const char* t_r = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)(t_r), (const char*)("")) != 0)) {
{
return t_r;
}
}
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))))) {
t_r = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[t_i])));
if ((strcmp((const char*)(t_r), (const char*)("")) != 0)) {
{
return t_r;
}
}
t_i += 1;
}
return "";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_r2 = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)(t_r2), (const char*)("")) != 0)) {
{
return t_r2;
}
}
return t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
return t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
return t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
}
}
return "";
}
const char* t_gui_ds_marker_block(ctron_list* t_b) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_hit = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[0]);
t_hit = "";
if ((strcmp((const char*)(t_st), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_b))->items[t_i]))) > 1)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])));
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Let")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[4])));
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("While")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])));
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("For")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])));
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("If")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])));
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_marker_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])));
}
}
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("If")) == 0)) {
{
t_hit = t_gui_ds_marker_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])));
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_hit = t_gui_ds_marker_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[1])));
}
}
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Expr")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])));
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Assign")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])));
if ((strcmp((const char*)(t_hit), (const char*)("")) == 0)) {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])));
}
}
}
}
else {
{
t_hit = t_gui_ds_marker_expr((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])));
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(t_hit), (const char*)("")) != 0)) {
{
return t_hit;
}
}
t_i += 1;
}
}
return "";
}
const char* t_gui_ds_marker_file(ctron_list* t_file) 
{
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_file))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_file))->items[t_i])))->items[0])), (const char*)("Fn")) == 0)) {
{
const char* t_hit = t_gui_ds_marker_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_file))->items[t_i])))->items[5])));
if ((strcmp((const char*)(t_hit), (const char*)("")) != 0)) {
{
return t_hit;
}
}
}
}
t_i += 1;
}
return "";
}
int32_t t_gui_ds_ordn_expr(ctron_list* t_e, const char* t_vname, ctron_list* t_names) 
{
if ((((ctron_list*)(t_e))->n < 1)) {
{
return 0;
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1])), (const char*)("run")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 2)) {
{
const char* t_a = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a))->items[0])), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1])), (const char*)(t_vname)) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))) > 1)) {
{
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))))) {
ctron_list_push((ctron_list*)(t_names), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])))->items[t_k])))->items[1]));
t_k += 1;
}
return 1;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname, t_names) > 0)) {
{
return 1;
}
}
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))))) {
if ((t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[t_i])), t_vname, t_names) > 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
if ((t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname, t_names) > 0)) {
{
return 1;
}
}
return t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vname, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
return t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname, t_names);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
return t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname, t_names);
}
}
return 0;
}
int32_t t_gui_ds_ordn_block(ctron_list* t_b, const char* t_vname, ctron_list* t_names) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
int32_t t_got = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[0]);
t_got = 0;
if ((strcmp((const char*)(t_st), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_b))->items[t_i]))) > 1)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname, t_names);
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Let")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[4])), t_vname, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("While")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("For")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])), t_vname, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("If")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname, t_names);
if ((t_got == 0)) {
{
t_got = t_gui_ds_ordn_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])), t_vname, t_names);
}
}
if ((t_got == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("If")) == 0)) {
{
t_got = t_gui_ds_ordn_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])), t_vname, t_names);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_got = t_gui_ds_ordn_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[1])), t_vname, t_names);
}
}
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Expr")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname, t_names);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Assign")) == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname, t_names);
if ((t_got == 0)) {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])), t_vname, t_names);
}
}
}
}
else {
{
t_got = t_gui_ds_ordn_expr((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])), t_vname, t_names);
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((t_got > 0)) {
{
return 1;
}
}
t_i += 1;
}
}
return 0;
}
ctron_list* t_gui_ds_ordn_file(ctron_list* t_file, const char* t_vname) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0)) {
{
ctron_list* t_names = (ctron_list*)(ctron_list_new());
if ((t_gui_ds_ordn_block((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_vname, t_names) > 0)) {
{
return t_names;
}
}
}
}
t_i += 1;
}
}
return ctron_list_new();
}
int32_t t_gui_ds_rew_expr(ctron_list* t_e, const char* t_vname) 
{
if ((((ctron_list*)(t_e))->n < 1)) {
{
return 0;
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1])), (const char*)("run")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 2)) {
{
const char* t_a = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a))->items[0])), (const char*)("Call")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[1]))) > 1)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])))->items[1])), (const char*)(t_vname)) == 0)) {
{
ctron_list* t_nn = (ctron_list*)(t_mk("Ident"));
ctron_list_push((ctron_list*)(t_nn), ctron_str_concat((const char*)("__gui_run_"), (const char*)(t_vname)));
((ctron_list*)(t_e))->items[1] = (char*)(t_nn);
ctron_list* t_na = (ctron_list*)(t_mk("Args"));
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(((const char*)((ctron_list*)(t_a))->items[2]))))) {
ctron_list_push((ctron_list*)(t_na), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])))->items[t_i])))->items[2]));
t_i += 1;
}
((ctron_list*)(t_e))->items[2] = (char*)(t_na);
return 1;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname) > 0)) {
{
return 1;
}
}
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))))) {
if ((t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[t_i])), t_vname) > 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
if ((t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname) > 0)) {
{
return 1;
}
}
return t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vname);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
return t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname);
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vname);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
return t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vname);
}
}
return 0;
}
int32_t t_gui_ds_rew_block(ctron_list* t_b, const char* t_vname) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
int32_t t_got = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[0]);
t_got = 0;
if ((strcmp((const char*)(t_st), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_b))->items[t_i]))) > 1)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname);
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Let")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[4])), t_vname);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("While")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("For")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])), t_vname);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("If")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname);
if ((t_got == 0)) {
{
t_got = t_gui_ds_rew_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[2])), t_vname);
}
}
if ((t_got == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("If")) == 0)) {
{
t_got = t_gui_ds_rew_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])), t_vname);
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_got = t_gui_ds_rew_block((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])))->items[1])), t_vname);
}
}
}
}
}
}
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Expr")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname);
}
}
else {
{
if ((strcmp((const char*)(t_st), (const char*)("Assign")) == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[1])), t_vname);
if ((t_got == 0)) {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])))->items[3])), t_vname);
}
}
}
}
else {
{
t_got = t_gui_ds_rew_expr((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])), t_vname);
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if ((t_got > 0)) {
{
return 1;
}
}
t_i += 1;
}
}
return 0;
}
int32_t t_gui_ds_rew_file(ctron_list* t_file, const char* t_vname) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0)) {
{
if ((t_gui_ds_rew_block((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_vname) > 0)) {
{
return 1;
}
}
}
}
t_i += 1;
}
}
return 0;
}
ctron_list* t_gui_ds_premerge(ctron_list* t_file, ctron_list* t_unit) 
{
ctron_list* t_vnames = (ctron_list*)(ctron_list_new());
ctron_list* t_vprops = (ctron_list*)(ctron_list_new());
t_gui_ds_views(t_file, t_vnames, t_vprops);
ctron_list* t_names = (ctron_list*)(ctron_list_new());
const char* t_vname = t_gui_ds_find_file(t_file, t_vnames, t_names);
ctron_list* t_ordn = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(t_vname), (const char*)("")) != 0)) {
{
int32_t t_oi = 1;
while ((t_oi < ((ctron_list*)(t_names))->n)) {
ctron_list_push((ctron_list*)(t_ordn), ((const char*)((ctron_list*)(t_names))->items[t_oi]));
t_oi += 1;
}
}
}
else {
{
t_vname = t_gui_ds_marker_file(t_file);
if ((strcmp((const char*)(t_vname), (const char*)("")) == 0)) {
{
return t_file;
}
}
t_names = (ctron_list*)(t_gui_ds_ordn_file(t_file, t_vname));
int32_t t_oi2 = 0;
while ((t_oi2 < ((ctron_list*)(t_names))->n)) {
ctron_list_push((ctron_list*)(t_ordn), ((const char*)((ctron_list*)(t_names))->items[t_oi2]));
t_oi2 += 1;
}
}
}
t_gui_ds_rew_file(t_file, t_vname);
ctron_list* t_nu = (ctron_list*)(t_mk("Use"));
ctron_list* t_nsegs = (ctron_list*)(t_mk("Segs"));
ctron_list_push((ctron_list*)(t_nsegs), "gui");
ctron_list* t_nsyms = (ctron_list*)(t_mk("Syms"));
ctron_list_push((ctron_list*)(t_nsyms), "rt_run_anchor");
ctron_list_push((ctron_list*)(t_nsyms), "rt_run_anchor");
ctron_list_push((ctron_list*)(t_nsyms), "ev_suffix_i");
ctron_list_push((ctron_list*)(t_nsyms), "ev_suffix_i");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_s");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_s");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_i");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_i");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_b");
ctron_list_push((ctron_list*)(t_nsyms), "ev_arg_b");
ctron_list_push((ctron_list*)(t_nu), (char*)(t_nsegs));
ctron_list_push((ctron_list*)(t_nu), (char*)(t_nsyms));
ctron_list_push((ctron_list*)(t_file), (char*)(t_nu));
const char* t_hdr = "__ordn__";
int32_t t_k2 = 0;
while ((t_k2 < ((ctron_list*)(t_ordn))->n)) {
if ((t_k2 > 0)) {
{
t_hdr = ctron_str_concat((const char*)(t_hdr), (const char*)(","));
}
}
t_hdr = ctron_str_concat((const char*)(t_hdr), (const char*)(((const char*)((ctron_list*)(t_ordn))->items[t_k2])));
t_k2 += 1;
}
ctron_list_push((ctron_list*)(t_unit), t_hdr);
return t_file;
}
const char* t_gui_ds_target(ctron_list* t_file) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
return ((const char*)((ctron_list*)(t_d))->items[2]);
}
}
}
}
t_i += 1;
}
}
return "";
}
ctron_list* t_gui_ds_sanitize(ctron_list* t_file) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_file))->items[0]));
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((ctron_len((const void*)(t_d)) > 2)) {
{
int t_keep = 0;
const char* t_tag = ((const char*)((ctron_list*)(t_d))->items[0]);
if ((strcmp((const char*)(t_tag), (const char*)("Fn")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("FnPub")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("FnC")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("FnExt")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Method")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Struct")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Enum")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Trait")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Impl")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Test")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("GuiBlock")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Use")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Static")) == 0)) {
{
t_keep = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Const")) == 0)) {
{
t_keep = 1;
}
}
if (t_keep) {
{
ctron_list_push((ctron_list*)(t_out), t_d);
}
}
}
}
t_i += 1;
}
}
return t_out;
}
ctron_list* t_gui_ds_postmerge(ctron_list* t_file, ctron_list* t_unit, ctron_list* t_diags) 
{
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return t_file;
}
}
if ((((ctron_list*)(t_unit))->n == 0)) {
{
return t_file;
}
}
const char* t_hdr = ((const char*)((ctron_list*)(t_unit))->items[0]);
if ((strlen((const char*)(t_hdr)) < 9)) {
{
return t_file;
}
}
if ((strcmp((const char*)(ctron_byte_slice(t_hdr, 0, 8)), (const char*)("__ordn__")) != 0)) {
{
return t_file;
}
}
const char* t_ordntxt = ctron_byte_slice(t_hdr, 8, strlen((const char*)(t_hdr)));
ctron_list* t_ordn = (ctron_list*)(ctron_list_new());
int32_t t_oi = 0;
int32_t t_os = 0;
while ((t_oi <= strlen((const char*)(t_ordntxt)))) {
if ((t_oi == strlen((const char*)(t_ordntxt)))) {
{
if ((t_oi > t_os)) {
{
ctron_list_push((ctron_list*)(t_ordn), ctron_byte_slice(t_ordntxt, t_os, t_oi));
}
}
t_oi += 1;
}
}
else {
{
if ((ctron_byte_at(t_ordntxt, t_oi) == 44)) {
{
ctron_list_push((ctron_list*)(t_ordn), ctron_byte_slice(t_ordntxt, t_os, t_oi));
t_os = (t_oi + 1);
}
}
t_oi += 1;
}
}
}
t_file = (ctron_list*)(t_gui_ds_sanitize(t_file));
const char* t_vname = t_gui_ds_target(t_file);
if ((strcmp((const char*)(t_vname), (const char*)("")) == 0)) {
{
return t_file;
}
}
const char* t_tgt = ((const char*)((ctron_list*)(t_file))->items[0]);
int t_found = 0;
ctron_list* t_nf = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_nf), ((const char*)((ctron_list*)(t_file))->items[0]));
int32_t t_ri = 1;
{
const char* t_rd = 0;
int t_is_tgt = 0;
while ((t_ri < ((ctron_list*)(t_file))->n)) {
t_rd = ((const char*)((ctron_list*)(t_file))->items[t_ri]);
t_is_tgt = 0;
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_rd))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_rd))->items[1])), (const char*)("view")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_rd))->items[2])), (const char*)(t_vname)) == 0)) {
{
t_is_tgt = 1;
}
}
}
}
}
}
if (t_is_tgt) {
{
t_tgt = t_rd;
t_found = 1;
}
}
else {
{
ctron_list_push((ctron_list*)(t_nf), t_rd);
}
}
t_ri += 1;
}
}
if (t_found) {
{
ctron_list_push((ctron_list*)(t_nf), t_tgt);
t_file = (ctron_list*)(t_nf);
}
}
ctron_list* t_vnames = (ctron_list*)(ctron_list_new());
ctron_list* t_vprops = (ctron_list*)(ctron_list_new());
t_gui_ds_views(t_file, t_vnames, t_vprops);
ctron_list* t_dn = (ctron_list*)(ctron_list_new());
ctron_list* t_dt = (ctron_list*)(ctron_list_new());
int32_t t_vi = 0;
while ((t_vi < ((ctron_list*)(t_vnames))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vnames))->items[t_vi])), (const char*)(t_vname)) == 0)) {
{
t_gui_props_typed(((const char*)((ctron_list*)(t_vprops))->items[t_vi]), t_dn, t_dt);
t_vi = ((ctron_list*)(t_vnames))->n;
}
}
t_vi += 1;
}
ctron_list* t_stns = (ctron_list*)(ctron_list_new());
ctron_list* t_sfk = (ctron_list*)(ctron_list_new());
ctron_list* t_stv = (ctron_list*)(ctron_list_new());
t_gui_field_tables(t_file, t_stns, t_sfk, t_stv);
ctron_list* t_paths = (ctron_list*)(ctron_list_new());
ctron_list* t_pty = (ctron_list*)(ctron_list_new());
ctron_list* t_lists = (ctron_list*)(ctron_list_new());
ctron_list* t_evn = (ctron_list*)(ctron_list_new());
ctron_list* t_evf = (ctron_list*)(ctron_list_new());
ctron_list* t_inps = (ctron_list*)(ctron_list_new());
ctron_list* t_ievs = (ctron_list*)(ctron_list_new());
ctron_list* t_crows = (ctron_list*)(ctron_list_new());
ctron_list* t_cty = (ctron_list*)(ctron_list_new());
ctron_list* t_cvn = (ctron_list*)(ctron_list_new());
ctron_list* t_cvf = (ctron_list*)(ctron_list_new());
ctron_list* t_ciev = (ctron_list*)(ctron_list_new());
t_gui_ds_collect_slots(t_file, t_dn, t_paths, t_pty, t_lists, t_evn, t_evf, t_inps, t_ievs, t_crows, t_cty, t_stns, t_sfk, t_stv, t_cvn, t_cvf, t_ciev);
ctron_list* t_made = (ctron_list*)(t_gui_ds_gen(t_file, t_vname, t_dn, t_dt, t_ordn, t_paths, t_pty, t_lists, t_evn, t_evf, t_inps, t_ievs, t_crows, t_cty, t_stns, t_sfk, t_stv, t_cvn, t_cvf, t_ciev));
int32_t t_k = 1;
while ((t_k < ((ctron_list*)(t_made))->n)) {
ctron_list_push((ctron_list*)(t_file), ((const char*)((ctron_list*)(t_made))->items[t_k]));
t_k += 1;
}
return t_file;
}
const char* t_pkg_dir_of(const char* t_path) 
{
int32_t t_i = (strlen((const char*)(t_path)) - 1);
while ((t_i >= 0)) {
if ((ctron_byte_at(t_path, t_i) == 47)) {
{
return ctron_byte_slice(t_path, 0, t_i);
}
}
t_i -= 1;
}
return ".";
}
int t_segs_std(ctron_list* t_d) 
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Use")) != 0)) {
{
return 0;
}
}
const char* t_segs = ((const char*)((ctron_list*)(t_d))->items[1]);
if ((ctron_len((const void*)(t_segs)) < 2)) {
{
return 0;
}
}
return t_or2(t_seq2(((const char*)((ctron_list*)(t_segs))->items[1]), "std"), t_seq2(((const char*)((ctron_list*)(t_segs))->items[1]), "stdweb"));
}
const char* t_cap_net_fine(const char* t_sym) 
{
if (t_or2(t_seq2(t_sym, "net_tcp_listen"), t_or2(t_seq2(t_sym, "net_unix_listen"), t_seq2(t_sym, "net_unix_accept")))) {
{
return "net.listen";
}
}
if (t_or2(t_seq2(t_sym, "net_tcp_connect"), t_or2(t_seq2(t_sym, "net_udp_socket"), t_or2(t_seq2(t_sym, "net_udp_bind"), t_or2(t_seq2(t_sym, "net_udp_sendto"), t_or2(t_seq2(t_sym, "net_udp_recvfrom"), t_seq2(t_sym, "net_unix_connect"))))))) {
{
return "net.connect";
}
}
if (t_or2(t_seq2(t_sym, "net_resolve"), t_seq2(t_sym, "net_resolve_all"))) {
{
return "net.resolve";
}
}
return "";
}
int t_pkg_is_prelude_sym(const char* t_nm) 
{
if (t_or2(t_seq2(t_nm, "parallel"), t_seq2(t_nm, "arena"))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "bit"), t_seq2(t_nm, "dom"))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "print"), t_or2(t_seq2(t_nm, "println"), t_or2(t_seq2(t_nm, "panic"), t_seq2(t_nm, "assert"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "assert_eq"), t_or2(t_seq2(t_nm, "assert_ne"), t_seq2(t_nm, "expect")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "fmt"), t_or2(t_seq2(t_nm, "byte_at"), t_or2(t_seq2(t_nm, "byte_slice"), t_seq2(t_nm, "char_len"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "read_file"), t_or2(t_seq2(t_nm, "read_dir"), t_or2(t_seq2(t_nm, "fs_exists"), t_or2(t_seq2(t_nm, "fs_write"), t_seq2(t_nm, "fs_delete")))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "now_ms"), t_or2(t_seq2(t_nm, "now_ms_text"), t_seq2(t_nm, "str_from_c")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "Some"), t_or2(t_seq2(t_nm, "None"), t_or2(t_seq2(t_nm, "Ok"), t_seq2(t_nm, "Err"))))) {
{
return 1;
}
}
return 0;
}
void t_pkg_check_caps(ctron_list* t_mf, const char* t_dir, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_mf))->n)) {
t_d = ((const char*)((ctron_list*)(t_mf))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Use")) == 0)) {
{
const char* t_segs = ((const char*)((ctron_list*)(t_d))->items[1]);
const char* t_symsN = ((const char*)((ctron_list*)(t_d))->items[2]);
const char* t_cap = "";
if (((ctron_len((const void*)(t_segs)) > 2) && t_seq2(((const char*)((ctron_list*)(t_segs))->items[1]), "std"))) {
{
t_cap = ((const char*)((ctron_list*)(t_segs))->items[2]);
}
}
else {
if ((ctron_len((const void*)(t_segs)) > 1)) {
{
t_cap = ((const char*)((ctron_list*)(t_segs))->items[1]);
}
}
}
if ((strlen((const char*)(t_cap)) > 0)) {
{
if (t_or2(t_or2(t_seq2(t_cap, "fs"), t_seq2(t_cap, "time")), t_or2(t_seq2(t_cap, "net"), t_seq2(t_cap, "db")))) {
{
const char* t_name = "";
if ((ctron_len((const void*)(t_symsN)) > 1)) {
{
t_name = ((const char*)((ctron_list*)(t_symsN))->items[1]);
}
}
else {
{
if ((ctron_len((const void*)(t_segs)) > 3)) {
{
t_name = ((const char*)((ctron_list*)(t_segs))->items[3]);
}
}
}
}
if ((strlen((const char*)(t_name)) > 0)) {
{
int t_hit = 0;
int32_t t_j = 1;
{
const char* t_dd = 0;
while (((t_j < ((ctron_list*)(t_mf))->n) && (!t_hit))) {
t_dd = ((const char*)((ctron_list*)(t_mf))->items[t_j]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_dd))->items[0])), (const char*)("Fn")) == 0)) {
{
const char* t_ps = ((const char*)((ctron_list*)(t_dd))->items[3]);
int32_t t_q = 1;
{
const char* t_pr = 0;
while ((t_q < ctron_len((const void*)(t_ps)))) {
t_pr = ((const char*)((ctron_list*)(t_ps))->items[t_q]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pr))->items[0])), (const char*)("Param")) == 0)) {
{
const char* t_pt = ((const char*)((ctron_list*)(t_pr))->items[3]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pt))->items[0])), (const char*)("Ref")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_pt))->items[1])))->items[0])), (const char*)("Named")) == 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_pt))->items[1])))->items[1]), t_name)) {
{
t_hit = 1;
}
}
}
}
}
}
}
}
t_q += 1;
}
}
}
}
t_j += 1;
}
}
int t_capok = t_pkg_caps_allowed(t_dir, t_cap);
if (((!t_capok) && t_seq2(t_cap, "net"))) {
{
if (t_or2(t_pkg_caps_allowed(t_dir, "net.listen"), t_or2(t_pkg_caps_allowed(t_dir, "net.connect"), t_pkg_caps_allowed(t_dir, "net.resolve")))) {
{
t_capok = 1;
}
}
}
}
if (((!t_capok) && t_seq2(t_cap, "db"))) {
{
if (t_pkg_caps_allowed(t_dir, "db.connect")) {
{
t_capok = 1;
}
}
}
}
if ((t_hit && (!t_capok))) {
{
t_diag2(t_diags, "E4010", "", t_cap, t_name);
}
}
ctron_list* t_symsF = (ctron_list*)(ctron_list_new());
int32_t t_kf = 1;
while ((t_kf < ctron_len((const void*)(t_symsN)))) {
ctron_list_push((ctron_list*)(t_symsF), ((const char*)((ctron_list*)(t_symsN))->items[t_kf]));
t_kf += 1;
}
if ((ctron_len((const void*)(t_segs)) > 3)) {
{
ctron_list_push((ctron_list*)(t_symsF), ((const char*)((ctron_list*)(t_segs))->items[3]));
}
}
int32_t t_sf = 0;
{
const char* t_fsym = 0;
int t_dupf = 0;
while ((t_sf < ((ctron_list*)(t_symsF))->n)) {
t_fsym = ((const char*)((ctron_list*)(t_symsF))->items[t_sf]);
t_dupf = 0;
int32_t t_df = 0;
while ((t_df < t_sf)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_symsF))->items[t_df])), (const char*)(t_fsym)) == 0)) {
{
t_dupf = 1;
}
}
t_df += 1;
}
if (t_dupf) {
{
t_sf += 1;
continue;
}
}
if (t_seq2(t_cap, "net")) {
{
const char* t_fk = t_cap_net_fine(t_fsym);
if (((strlen((const char*)(t_fk)) > 0) && (!t_pkg_caps_allowed(t_dir, t_fk)))) {
{
t_diag2(t_diags, "E4010", "", t_fk, t_fsym);
}
}
}
}
if (t_seq2(t_cap, "db")) {
{
if ((!t_pkg_caps_allowed(t_dir, "db.connect"))) {
{
t_diag2(t_diags, "E4010", "", "db.connect", t_fsym);
}
}
}
}
t_sf += 1;
}
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
int t_pkg_ctcl_at(const char* t_t, int32_t t_i, const char* t_key) 
{
int32_t t_k = 0;
while ((t_k < strlen((const char*)(t_key)))) {
if (t_or2(((t_i + t_k) >= strlen((const char*)(t_t))), (ctron_byte_at(t_t, (t_i + t_k)) != ctron_byte_at(t_key, t_k)))) {
{
return 0;
}
}
t_k += 1;
}
return 1;
}
int t_pkg_ctcl_pre(const char* t_t, int32_t t_i) 
{
if ((t_i == 0)) {
{
return 1;
}
}
int32_t t_p = ctron_byte_at(t_t, (t_i - 1));
if (t_or2(t_or2(((t_p >= 97) && (t_p <= 122)), (t_p == 95)), ((t_p >= 48) && (t_p <= 57)))) {
{
return 0;
}
}
return 1;
}
int t_pkg_caps_allowed(const char* t_dir, const char* t_cap) 
{
const char* t_t = "";
const char* t_mv = (const char*)(ctron_read_file(ctron_str_concat((const char*)(t_dir), (const char*)("/../Ctron.ctcl"))));
if (t_mv != NULL) {
const char* t_x = t_mv;
{
t_t = t_x;
if ((strlen((const char*)(t_t)) == 0)) {
{
return 1;
}
}
}
}
else {
{
return 1;
}
}
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_t)))) {
if ((t_pkg_ctcl_at(t_t, t_i, "caps") && t_pkg_ctcl_pre(t_t, t_i))) {
{
int32_t t_j = (t_i + 4);
while (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) == 32))) {
t_j += 1;
}
if (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) == 61))) {
{
t_j += 1;
while (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) == 32))) {
t_j += 1;
}
if (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) == 91))) {
{
t_j += 1;
while (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) != 93))) {
if ((ctron_byte_at(t_t, t_j) == 34)) {
{
const char* t_nm = "";
t_j += 1;
while (((t_j < strlen((const char*)(t_t))) && (ctron_byte_at(t_t, t_j) != 34))) {
t_nm = ctron_str_concat((const char*)(t_nm), (const char*)(ctron_byte_slice(t_t, t_j, (t_j + 1))));
t_j += 1;
}
t_j += 1;
if (t_seq2(t_nm, t_cap)) {
{
return 1;
}
}
}
}
else {
{
t_j += 1;
}
}
}
return 0;
}
}
}
}
}
}
t_i += 1;
}
return 0;
}
const char* t_pkg_exe_dir(const char* t_exe) 
{
int32_t t_i = (strlen((const char*)(t_exe)) - 1);
while ((t_i >= 0)) {
if ((ctron_byte_at(t_exe, t_i) == 47)) {
{
return ctron_byte_slice(t_exe, 0, t_i);
}
}
t_i -= 1;
}
return "";
}
int t_pkg_std_installed(const char* t_dir) 
{
if ((strlen((const char*)(ctron_env_get("CTRON_STDPATH"))) > 0)) {
{
return 1;
}
}
const char* t_exe = ctron_exe_path();
if ((strlen((const char*)(t_exe)) > 0)) {
{
const char* t_root = ctron_str_concat((const char*)(t_pkg_exe_dir(t_exe)), (const char*)("/../lib/ctron/std"));
if ((ctron_fs_exists(ctron_str_concat((const char*)(t_root), (const char*)("/str.ct")))?1:0)) {
{
return 1;
}
}
}
}
return 0;
}
const char* t_pkg_std_root(const char* t_dir) 
{
const char* t_envp = ctron_env_get("CTRON_STDPATH");
if ((strlen((const char*)(t_envp)) > 0)) {
{
return t_envp;
}
}
const char* t_exe = ctron_exe_path();
if ((strlen((const char*)(t_exe)) > 0)) {
{
const char* t_root = ctron_str_concat((const char*)(t_pkg_exe_dir(t_exe)), (const char*)("/../lib/ctron/std"));
if ((ctron_fs_exists(ctron_str_concat((const char*)(t_root), (const char*)("/str.ct")))?1:0)) {
{
return t_root;
}
}
}
}
return ctron_str_concat((const char*)(t_dir), (const char*)("/../std"));
}
ctron_list* t_pkg_load_use(ctron_list* t_file, const char* t_dir, ctron_list* t_stack, ctron_list* t_diags) 
{
ctron_list* t_unit = (ctron_list*)(ctron_list_new());
ctron_list* t_f2 = (ctron_list*)(t_gui_ds_premerge(t_file, t_unit));
ctron_list* t_out = (ctron_list*)(t_pkg_load_use_done(t_f2, t_dir, t_stack, ctron_list_new(), t_diags, 1));
return t_gui_ds_postmerge(t_out, t_unit, t_diags);
}
int t_pkg_is_named_kind(const char* t_t) 
{
return t_or2((strcmp((const char*)(t_t), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnPub")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Struct")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Enum")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnExt")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Static")) == 0), (strcmp((const char*)(t_t), (const char*)("Const")) == 0)))))));
}
int t_pkg_refs_walk(ctron_list* t_d, ctron_list* t_ktags, ctron_list* t_kshapes, ctron_list* t_acc, int32_t t_depth) 
{
if ((t_depth > 4000)) {
{
return 0;
}
}
const char* t_sh = t_ast_shape(t_ktags, t_kshapes, ((const char*)((ctron_list*)(t_d))->items[0]));
if ((strcmp((const char*)(t_sh), (const char*)("?")) == 0)) {
{
return 0;
}
}
int32_t t_i = 1;
{
const char* t_kind = 0;
while ((t_i < ((ctron_list*)(t_d))->n)) {
t_kind = t_ast_kind(t_sh, (t_i - 1));
if ((strcmp((const char*)(t_kind), (const char*)("S")) == 0)) {
{
ctron_list_push((ctron_list*)(t_acc), ((const char*)((ctron_list*)(t_d))->items[t_i]));
}
}
else {
{
if ((strcmp((const char*)(t_kind), (const char*)("N")) == 0)) {
{
if ((!t_pkg_refs_walk((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[t_i])), t_ktags, t_kshapes, t_acc, (t_depth + 1)))) {
{
return 0;
}
}
}
}
else {
{
return 0;
}
}
}
}
t_i += 1;
}
}
return 1;
}
ctron_list* t_pkg_load_use_done(ctron_list* t_file, const char* t_dir, ctron_list* t_stack, ctron_list* t_done, ctron_list* t_diags, int t_top) 
{
t_pkg_check_caps(t_file, t_dir, t_diags);
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return t_file;
}
}
ctron_list* t_out = (ctron_list*)(t_file);
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_out))->n)) {
t_d = ((const char*)((ctron_list*)(t_out))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Use")) == 0)) {
{
int t_isstd = t_segs_std((ctron_list*)(t_d));
const char* t_segs = ((const char*)((ctron_list*)(t_d))->items[1]);
const char* t_symsN = ((const char*)((ctron_list*)(t_d))->items[2]);
ctron_list* t_syms = (ctron_list*)(ctron_list_new());
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(t_symsN)))) {
ctron_list_push((ctron_list*)(t_syms), ((const char*)((ctron_list*)(t_symsN))->items[t_k]));
t_k += 1;
}
if ((((ctron_list*)(t_syms))->n == 0)) {
{
ctron_list_push((ctron_list*)(t_syms), ((const char*)((ctron_list*)(t_segs))->items[(ctron_len((const void*)(t_segs)) - 1)]));
ctron_list_push((ctron_list*)(t_syms), ((const char*)((ctron_list*)(t_segs))->items[(ctron_len((const void*)(t_segs)) - 1)]));
}
}
const char* t_mpath = "";
if (t_isstd) {
{
t_mpath = t_pkg_std_root(t_dir);
int32_t t_j2 = 2;
while ((t_j2 < ctron_len((const void*)(t_segs)))) {
t_mpath = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_mpath), (const char*)("/"))), (const char*)(((const char*)((ctron_list*)(t_segs))->items[t_j2])));
t_j2 += 1;
}
t_mpath = ctron_str_concat((const char*)(t_mpath), (const char*)(".ct"));
if ((!(ctron_fs_exists(t_mpath)?1:0))) {
{
if (t_pkg_std_installed(t_dir)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_str_concat((const char*)(ctron_str_concat((const char*)("W8901: std 模块缺失:"), (const char*)(t_mpath))), (const char*)("(安装损坏或 CTRON_STDPATH 配错)")));
return t_out;
}
}
}
}
}
}
else {
{
const char* t_rel = "";
if ((ctron_len((const void*)(t_segs)) == 2)) {
{
t_rel = ctron_str_concat((const char*)("/"), (const char*)(((const char*)((ctron_list*)(t_segs))->items[1])));
}
}
else {
{
int32_t t_j2b = 2;
while ((t_j2b < ctron_len((const void*)(t_segs)))) {
t_rel = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_rel), (const char*)("/"))), (const char*)(((const char*)((ctron_list*)(t_segs))->items[t_j2b])));
t_j2b += 1;
}
}
}
t_mpath = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_dir), (const char*)(t_rel))), (const char*)(".ct"));
if ((!(ctron_fs_exists(t_mpath)?1:0))) {
{
const char* t_sroot = t_pkg_std_root(t_dir);
if ((strlen((const char*)(t_sroot)) > 0)) {
{
int32_t t_cut = (0 - 1);
int32_t t_ci = (strlen((const char*)(t_sroot)) - 1);
while ((t_ci >= 0)) {
if ((ctron_byte_at(t_sroot, t_ci) == 47)) {
{
t_cut = t_ci;
break;
}
}
t_ci -= 1;
}
if ((t_cut > 0)) {
{
const char* t_dpath = ctron_byte_slice(t_sroot, 0, t_cut);
int32_t t_j2c = 1;
while ((t_j2c < ctron_len((const void*)(t_segs)))) {
t_dpath = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_dpath), (const char*)("/"))), (const char*)(((const char*)((ctron_list*)(t_segs))->items[t_j2c])));
t_j2c += 1;
}
t_dpath = ctron_str_concat((const char*)(t_dpath), (const char*)(".ct"));
if ((ctron_fs_exists(t_dpath)?1:0)) {
{
t_mpath = t_dpath;
}
}
}
}
}
}
}
}
if ((!(ctron_fs_exists(t_mpath)?1:0))) {
{
const char* t_apath = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_dir), (const char*)("/deps/"))), (const char*)(((const char*)((ctron_list*)(t_segs))->items[1])))), (const char*)(".ctart/impl"))), (const char*)(t_rel))), (const char*)(".ast"));
if ((ctron_fs_exists(t_apath)?1:0)) {
{
t_mpath = t_apath;
}
}
}
}
}
}
int t_isdone = t_in_list(t_done, t_mpath);
if ((t_isdone && (!t_top))) {
{
ctron_list* t_nm3 = (ctron_list*)(t_mk("File"));
int32_t t_j3 = 1;
while ((t_j3 < ((ctron_list*)(t_out))->n)) {
if ((t_j3 != t_i)) {
{
ctron_list_push((ctron_list*)(t_nm3), ((const char*)((ctron_list*)(t_out))->items[t_j3]));
}
}
t_j3 += 1;
}
t_out = (ctron_list*)(t_nm3);
t_i = 1;
continue;
}
}
int t_redo = (t_isdone && t_top);
if (((!t_isdone) && t_in_list(t_stack, t_mpath))) {
{
t_diag1(t_diags, "E5020.circ", "", t_mpath);
return t_out;
}
}
const char* t_src = ctron_read_file(t_mpath);
const char* t_mv = (const char*)(t_src);
if (t_mv != NULL) {
const char* t_ms = t_mv;
{
ctron_list* t_mf = (ctron_list*)(ctron_list_new());
int t_is_ast = (strlen((const char*)(t_mpath)) >= 4);
if (t_is_ast) {
{
if ((ctron_byte_at(t_mpath, (strlen((const char*)(t_mpath)) - 4)) == 46)) {
{
t_is_ast = (strcmp((const char*)(ctron_byte_slice(t_mpath, (strlen((const char*)(t_mpath)) - 4), strlen((const char*)(t_mpath)))), (const char*)(".ast")) == 0);
}
}
else {
{
t_is_ast = 0;
}
}
}
}
if (t_is_ast) {
{
ctron_cell* t_acur = (ctron_cell*)(ctron_cell_new(0));
t_mf = (ctron_list*)(t_ast_load_node(t_ms, t_acur));
}
}
else {
{
ctron_list* t_toks2 = (ctron_list*)(ctron_list_new());
ctron_list* t_lns2 = (ctron_list*)(ctron_list_new());
ctron_list* t_cols2 = (ctron_list*)(ctron_list_new());
ctron_list* t_pd2 = (ctron_list*)(ctron_list_new());
t_scan4(t_ms, t_toks2, t_lns2, t_cols2, t_pd2);
ctron_cell* t_cur2 = (ctron_cell*)(ctron_cell_new(0));
t_mf = (ctron_list*)(t_p_file(t_toks2, t_cur2, t_lns2, t_cols2, t_pd2));
}
}
t_pkg_check_caps(t_mf, t_dir, t_diags);
int32_t t_k3 = 0;
{
const char* t_sn = 0;
const char* t__al = 0;
int t_found = 0;
int t_vis = 0;
while ((t_k3 < ((ctron_list*)(t_syms))->n)) {
t_sn = ((const char*)((ctron_list*)(t_syms))->items[t_k3]);
t__al = ((const char*)((ctron_list*)(t_syms))->items[(t_k3 + 1)]);
t_found = 0;
t_vis = 0;
int32_t t_m2 = 1;
{
const char* t_md = 0;
while ((t_m2 < ((ctron_list*)(t_mf))->n)) {
t_md = ((const char*)((ctron_list*)(t_mf))->items[t_m2]);
if (((ctron_len((const void*)(t_md)) > 1) && (strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[1])), (const char*)(t_sn)) == 0))) {
{
t_found = 1;
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("FnPub")) == 0)) {
{
t_vis = 1;
}
}
if ((ctron_len((const void*)(t_md)) > 2)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[(ctron_len((const void*)(t_md)) - 1)])), (const char*)("pub")) == 0)) {
{
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Struct")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Enum")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("FnExt")) == 0)))) {
{
t_vis = 1;
}
}
}
}
}
}
}
}
t_m2 += 1;
}
}
if (((!t_found) && t_pkg_is_prelude_sym(t_sn))) {
{
t_found = 1;
t_vis = 1;
}
}
if ((!t_found)) {
{
t_diag1(t_diags, "E2020.use.miss", "", t_sn);
}
}
else {
if ((!t_vis)) {
{
t_diag1(t_diags, "E2020.use.priv", "", t_sn);
}
}
}
t_k3 += 2;
}
}
int32_t t_k8 = 0;
while ((t_k8 < ((ctron_list*)(t_syms))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_syms))->items[(t_k8 + 1)])), (const char*)(((const char*)((ctron_list*)(t_syms))->items[t_k8]))) != 0)) {
{
int32_t t_m9 = 1;
while ((t_m9 < ((ctron_list*)(t_out))->n)) {
if (((ctron_len((const void*)(((const char*)((ctron_list*)(t_out))->items[t_m9]))) > 1) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_out))->items[t_m9])))->items[1])), (const char*)(((const char*)((ctron_list*)(t_syms))->items[(t_k8 + 1)]))) == 0))) {
{
t_diag1(t_diags, "E5035", "", ((const char*)((ctron_list*)(t_syms))->items[(t_k8 + 1)]));
return t_out;
}
}
t_m9 += 1;
}
}
}
t_k8 += 2;
}
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return t_out;
}
}
ctron_list* t_stack2 = (ctron_list*)(t_stack);
ctron_list_push((ctron_list*)(t_stack2), t_mpath);
t_mf = (ctron_list*)(t_pkg_load_use_done(t_mf, t_dir, t_stack2, t_done, t_diags, 0));
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return t_out;
}
}
ctron_list* t_ktags = (ctron_list*)(ctron_list_new());
ctron_list* t_kshapes = (ctron_list*)(ctron_list_new());
t_ast_tables(t_ktags, t_kshapes);
ctron_list* t_keep = (ctron_list*)(ctron_list_new());
int32_t t_k9 = 0;
while ((t_k9 < ((ctron_list*)(t_syms))->n)) {
ctron_list_push((ctron_list*)(t_keep), ((const char*)((ctron_list*)(t_syms))->items[t_k9]));
t_k9 += 2;
}
int t_refs_ok = 1;
int t_grow = 1;
while (t_grow) {
t_grow = 0;
ctron_list* t_acc = (ctron_list*)(ctron_list_new());
int32_t t_j9 = 1;
{
int t_carry = 0;
while ((t_j9 < ((ctron_list*)(t_mf))->n)) {
t_carry = 0;
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j9])))->items[0])), (const char*)("Use")) != 0)) {
{
t_carry = 1;
if (t_pkg_is_named_kind(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j9])))->items[0]))) {
{
t_carry = 0;
int32_t t_ka = 0;
while ((t_ka < ((ctron_list*)(t_keep))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_keep))->items[t_ka])), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j9])))->items[1]))) == 0)) {
{
t_carry = 1;
}
}
t_ka += 1;
}
}
}
}
}
if (t_carry) {
{
if ((!t_pkg_refs_walk((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j9])), t_ktags, t_kshapes, t_acc, 0))) {
{
t_refs_ok = 0;
}
}
}
}
t_j9 += 1;
}
}
if ((!t_refs_ok)) {
{
t_grow = 0;
}
}
if (t_refs_ok) {
{
int32_t t_j10 = 1;
while ((t_j10 < ((ctron_list*)(t_mf))->n)) {
if (((ctron_len((const void*)(((const char*)((ctron_list*)(t_mf))->items[t_j10]))) > 1) && t_pkg_is_named_kind(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j10])))->items[0])))) {
{
int t_ink2 = 0;
int32_t t_kb = 0;
while ((t_kb < ((ctron_list*)(t_keep))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_keep))->items[t_kb])), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j10])))->items[1]))) == 0)) {
{
t_ink2 = 1;
}
}
t_kb += 1;
}
if ((!t_ink2)) {
{
int32_t t_kc = 0;
while ((t_kc < ((ctron_list*)(t_acc))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_acc))->items[t_kc])), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j10])))->items[1]))) == 0)) {
{
ctron_list_push((ctron_list*)(t_keep), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j10])))->items[1]));
t_grow = 1;
}
}
t_kc += 1;
}
}
}
}
}
t_j10 += 1;
}
}
}
}
ctron_list* t_merged = (ctron_list*)(t_mk("File"));
int32_t t_j3 = 1;
while ((t_j3 < ((ctron_list*)(t_out))->n)) {
if ((t_j3 != t_i)) {
{
ctron_list_push((ctron_list*)(t_merged), ((const char*)((ctron_list*)(t_out))->items[t_j3]));
}
}
t_j3 += 1;
}
int32_t t_j4 = 1;
while ((t_j4 < ((ctron_list*)(t_mf))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[0])), (const char*)("Use")) != 0)) {
{
int t_named = t_pkg_is_named_kind(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[0]));
if ((t_named && (!t_refs_ok))) {
{
}
}
else {
if (t_named) {
{
int t_ink3 = 0;
int32_t t_kd = 0;
while ((t_kd < ((ctron_list*)(t_keep))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_keep))->items[t_kd])), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[1]))) == 0)) {
{
t_ink3 = 1;
}
}
t_kd += 1;
}
if ((!t_ink3)) {
{
t_j4 += 1;
continue;
}
}
}
}
}
const char* t_rn = "";
int32_t t_k6 = 0;
while ((t_k6 < ((ctron_list*)(t_syms))->n)) {
if ((((ctron_len((const void*)(((const char*)((ctron_list*)(t_mf))->items[t_j4]))) > 1) && (strcmp((const char*)(((const char*)((ctron_list*)(t_syms))->items[t_k6])), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[1]))) == 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(t_syms))->items[(t_k6 + 1)])), (const char*)(((const char*)((ctron_list*)(t_syms))->items[t_k6]))) != 0))) {
{
t_rn = ((const char*)((ctron_list*)(t_syms))->items[(t_k6 + 1)]);
}
}
t_k6 += 2;
}
const char* t_iname = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[1]);
if ((strcmp((const char*)(t_rn), (const char*)("")) != 0)) {
{
t_iname = t_rn;
}
}
int t_dup = 0;
int32_t t_j5 = 1;
while ((t_j5 < ((ctron_list*)(t_merged))->n)) {
if (t_pkg_is_named_kind(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_merged))->items[t_j5])))->items[0]))) {
{
if (t_pkg_is_named_kind(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[0]))) {
{
if ((((ctron_len((const void*)(((const char*)((ctron_list*)(t_mf))->items[t_j4]))) > 1) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_merged))->items[t_j5]))) > 1)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_merged))->items[t_j5])))->items[1])), (const char*)(t_iname)) == 0))) {
{
t_dup = 1;
}
}
}
}
}
}
t_j5 += 1;
}
if (t_dup) {
{
if (t_redo) {
{
t_j4 += 1;
continue;
}
}
t_diag1(t_diags, "E5030", "", t_iname);
return t_out;
}
}
if ((strcmp((const char*)(t_rn), (const char*)("")) != 0)) {
{
ctron_list* t_nd = (ctron_list*)(t_mk(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[0])));
ctron_list_push((ctron_list*)(t_nd), t_rn);
int32_t t_k7 = 2;
while ((t_k7 < ctron_len((const void*)(((const char*)((ctron_list*)(t_mf))->items[t_j4]))))) {
ctron_list_push((ctron_list*)(t_nd), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_mf))->items[t_j4])))->items[t_k7]));
t_k7 += 1;
}
ctron_list_push((ctron_list*)(t_merged), (char*)(t_nd));
}
}
else {
{
ctron_list_push((ctron_list*)(t_merged), ((const char*)((ctron_list*)(t_mf))->items[t_j4]));
}
}
}
}
t_j4 += 1;
}
t_out = (ctron_list*)(t_merged);
t_i = 0;
if ((!t_redo)) {
{
ctron_list_push((ctron_list*)(t_done), t_mpath);
}
}
}
}
else {
{
if ((!t_isstd)) {
{
t_diag1(t_diags, "E2020.use.read", "", t_mpath);
return t_out;
}
}
int32_t t_k13 = 0;
while (((t_k13 + 1) < ((ctron_list*)(t_syms))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_syms))->items[(t_k13 + 1)])), (const char*)(((const char*)((ctron_list*)(t_syms))->items[t_k13]))) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_str_concat((const char*)("E2020.use.nat: "), (const char*)(t_diag_text1("E2020.use.nat", ((const char*)((ctron_list*)(t_syms))->items[(t_k13 + 1)])))));
return t_out;
}
}
t_k13 += 2;
}
}
}
}
}
t_i += 1;
}
}
return t_out;
}
const char* t_ast_read_line(const char* t_ser, ctron_cell* t_cur) 
{
const char* t_out = "";
int32_t t_i = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
while (((t_i < strlen((const char*)(t_ser))) && (ctron_byte_at(t_ser, t_i) != 10))) {
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_ser, t_i, (t_i + 1))));
t_i += 1;
}
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)((t_i + 1)), __ATOMIC_SEQ_CST);
return t_out;
}
ctron_list* t_ast_load_node(const char* t_ser, ctron_cell* t_cur) 
{
const char* t_line = t_ast_read_line(t_ser, t_cur);
int32_t t_sp = 2;
while ((t_sp < strlen((const char*)(t_line)))) {
if ((ctron_byte_at(t_line, t_sp) == 32)) {
{
break;
}
}
t_sp += 1;
}
const char* t_tag = ctron_byte_slice(t_line, 2, t_sp);
int32_t t_n = t_dvi(ctron_byte_slice(t_line, (t_sp + 1), strlen((const char*)(t_line))));
ctron_list* t_node = (ctron_list*)(t_mk(t_tag));
int32_t t_i = 0;
{
const char* t_rec = 0;
while ((t_i < t_n)) {
t_rec = t_ast_read_line(t_ser, t_cur);
if ((ctron_byte_at(t_rec, 0) == 83)) {
{
int32_t t_len = t_dvi(ctron_byte_slice(t_rec, 2, strlen((const char*)(t_rec))));
int32_t t_p0 = __atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST);
ctron_list_push((ctron_list*)(t_node), ctron_byte_slice(t_ser, t_p0, (t_p0 + t_len)));
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)(((t_p0 + t_len) + 1)), __ATOMIC_SEQ_CST);
}
}
else {
{
__atomic_store_n(&((ctron_cell*)(t_cur))->v, (int32_t)(((__atomic_load_n(&((ctron_cell*)(t_cur))->v, __ATOMIC_SEQ_CST) - strlen((const char*)(t_rec))) - 1)), __ATOMIC_SEQ_CST);
ctron_list_push((ctron_list*)(t_node), (char*)(t_ast_load_node(t_ser, t_cur)));
}
}
t_i += 1;
}
}
return t_node;
}
int t_is_opt_ret(ctron_list* t_ty) 
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Optional")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Named")) == 0)) {
{
const char* t_h = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (t_or2((strcmp((const char*)(t_h), (const char*)("Option")) == 0), (strcmp((const char*)(t_h), (const char*)("Result")) == 0))) {
{
return 1;
}
}
}
}
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("TypeArgs")) == 0) && (((ctron_list*)(t_ty))->n > 1)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])))->items[0])), (const char*)("Named")) == 0))) {
{
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])))->items[1])), (const char*)("Option")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])))->items[1])), (const char*)("Result")) == 0))) {
{
return 1;
}
}
}
}
return 0;
}
int t_is_prelude_name(const char* t_n) 
{
if (t_or2((strcmp((const char*)(t_n), (const char*)("I8")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("I16")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("I32")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("I64")) == 0), (strcmp((const char*)(t_n), (const char*)("ISize")) == 0)))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("U8")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("U16")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("U32")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("U64")) == 0), (strcmp((const char*)(t_n), (const char*)("USize")) == 0)))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("F32")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("F64")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Bool")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Str")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("String")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Void")) == 0), (strcmp((const char*)(t_n), (const char*)("Never")) == 0)))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("TaskPanic")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Channel")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("List")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Map")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Set")) == 0), (strcmp((const char*)(t_n), (const char*)("Box")) == 0))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("StringBuilder")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Atomic")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Global")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Mutex")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Sender")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Receiver")) == 0), (strcmp((const char*)(t_n), (const char*)("Task")) == 0)))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("Scope")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Arena")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Region")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Pool")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("ArenaList")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Simd")) == 0), (strcmp((const char*)(t_n), (const char*)("AnyError")) == 0)))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("Parallel")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Path")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Bytes")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Option")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Result")) == 0), (strcmp((const char*)(t_n), (const char*)("Bit")) == 0))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("Some")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("None")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Ok")) == 0), (strcmp((const char*)(t_n), (const char*)("Err")) == 0))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("Error")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Show")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Eq")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Drop")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Clone")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Hash")) == 0), (strcmp((const char*)(t_n), (const char*)("Iter")) == 0)))))))) {
{
return 1;
}
}
if (t_or2((strcmp((const char*)(t_n), (const char*)("Cap")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Clock")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Fs")) == 0), t_or2((strcmp((const char*)(t_n), (const char*)("Net")) == 0), (strcmp((const char*)(t_n), (const char*)("Log")) == 0)))))) {
{
return 1;
}
}
return 0;
}
int t_is_cmp_op(const char* t_op) 
{
return t_or2((strcmp((const char*)(t_op), (const char*)("Lt")) == 0), t_or2((strcmp((const char*)(t_op), (const char*)("Gt")) == 0), t_or2((strcmp((const char*)(t_op), (const char*)("Le")) == 0), t_or2((strcmp((const char*)(t_op), (const char*)("Ge")) == 0), t_or2((strcmp((const char*)(t_op), (const char*)("Eq")) == 0), (strcmp((const char*)(t_op), (const char*)("Ne")) == 0))))));
}
int t_ext_cb_target(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnExt")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)(t_nm)) == 0))) {
{
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(t_ps)))) {
if (((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ps))->items[t_k])))->items[0])), (const char*)("Param")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ps))->items[t_k])))->items[3])))->items[0])), (const char*)("FnType")) == 0))) {
{
return 1;
}
}
t_k += 1;
}
return 0;
}
}
t_i += 1;
}
}
return 0;
}
void t_walk_e(ctron_list* t_file, ctron_list* t_e, int t_ns, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if (((t_ns && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[3])), (const char*)("spawn")) == 0))) {
{
ctron_list_push((ctron_list*)(t_out), t_diag_render0("E4030", t_nline(t_e)));
}
}
if (((((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_cal))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[0])), (const char*)("Ident")) == 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1])), (const char*)("Channel")) == 0))) {
{
const char* t_ag2 = ((const char*)((ctron_list*)(t_cal))->items[2]);
if ((ctron_len((const void*)(t_ag2)) > 1)) {
{
const char* t_el = ((const char*)((ctron_list*)(t_ag2))->items[1]);
if ((!t_send_of(t_file, (ctron_list*)(t_el), 0))) {
{
ctron_list_push((ctron_list*)(t_out), t_diag_render0("E3020.elem", t_nline(t_e)));
}
}
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
const char* t_agx = ((const char*)((ctron_list*)(t_e))->items[2]);
int t_hasclo = 0;
int32_t t_qi = 1;
while (((t_qi < ctron_len((const void*)(t_agx))) && (!t_hasclo))) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_agx))->items[t_qi])))->items[0])), (const char*)("Closure")) == 0)) {
{
t_hasclo = 1;
}
}
t_qi += 1;
}
if ((t_hasclo && t_ext_cb_target(t_file, ((const char*)((ctron_list*)(t_cal))->items[1])))) {
{
ctron_list_push((ctron_list*)(t_out), t_diag_render1("E4042", t_nline(t_e), ((const char*)((ctron_list*)(t_cal))->items[1])));
}
}
}
}
t_walk_e(t_file, (ctron_list*)(t_cal), t_ns, t_fns, t_rets, t_out);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_ns, t_fns, t_rets, t_out);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
if (((t_is_cmp_op(((const char*)((ctron_list*)(t_e))->items[1])) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[0])), (const char*)("Binary")) == 0)) && t_is_cmp_op(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1])))) {
{
ctron_list_push((ctron_list*)(t_out), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E1001@"), (const char*)(t_nline(t_e)))), (const char*)("(parse): "))), (const char*)(t_diag_text0("E1001.chain"))));
}
}
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_ns, t_fns, t_rets, t_out);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out, 0);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out, 0);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ns, t_fns, t_rets, t_out);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
{
const char* t_a = 0;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_a = ((const char*)((ctron_list*)(t_arms))->items[t_i3]);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])), t_ns, t_fns, t_rets, t_out);
t_i3 += 1;
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ns, t_fns, t_rets, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out, 0);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ns, t_fns, t_rets, t_out, 0);
return;
}
}
}
void t_w8020_check(ctron_list* t_file, ctron_list* t_ce, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out) 
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ce))->items[0])), (const char*)("Call")) != 0)) {
{
return;
}
}
const char* t_cal2 = ((const char*)((ctron_list*)(t_ce))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal2))->items[0])), (const char*)("Ident")) != 0)) {
{
return;
}
}
const char* t_nm2 = ((const char*)((ctron_list*)(t_cal2))->items[1]);
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_fns))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_fns))->items[t_k])), (const char*)(t_nm2)) == 0)) {
{
if (t_is_opt_ret((ctron_list*)(((const char*)((ctron_list*)(t_rets))->items[t_k])))) {
{
ctron_list_push((ctron_list*)(t_out), t_diag_render1("W8020", t_nline(t_ce), t_nm2));
}
}
return;
}
}
t_k += 1;
}
}
void t_walk_b(ctron_list* t_file, ctron_list* t_b, int t_ns, ctron_list* t_fns, ctron_list* t_rets, ctron_list* t_out, int t_body) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ns, t_fns, t_rets, t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_lp = ((const char*)((ctron_list*)(t_st))->items[2]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_lp))->items[0])), (const char*)("PatId")) == 0) && t_is_prelude_name(((const char*)((ctron_list*)(t_lp))->items[1])))) {
{
ctron_list_push((ctron_list*)(t_out), t_diag_render1("W8040", t_nline((ctron_list*)(t_st)), ((const char*)((ctron_list*)(t_lp))->items[1])));
}
}
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_ns, t_fns, t_rets, t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ns, t_fns, t_rets, t_out);
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ns, t_fns, t_rets, t_out, 0);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ns, t_fns, t_rets, t_out);
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ns, t_fns, t_rets, t_out, 0);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ns, t_fns, t_rets, t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_w8020_check(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_fns, t_rets, t_out);
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ns, t_fns, t_rets, t_out);
}
}
}
}
}
}
}
t_i += 1;
}
}
if (t_body) {
{
t_w8020_check(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_fns, t_rets, t_out);
}
}
t_walk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_ns, t_fns, t_rets, t_out);
}
int t_s_prim(const char* t_n) 
{
if ((strcmp((const char*)(t_n), (const char*)("I8")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("I16")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("I32")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("I64")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("ISize")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("U8")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("U16")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("U32")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("U64")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("USize")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("F32")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("F64")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Bool")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Str")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Void")) == 0)) {
{
return 1;
}
}
return 0;
}
int t_s_special(const char* t_n) 
{
if ((strcmp((const char*)(t_n), (const char*)("Mutex")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Atomic")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Global")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Sender")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Receiver")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Task")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("String")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Box")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Never")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Option")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_n), (const char*)("Result")) == 0)) {
{
return 1;
}
}
return 0;
}
ctron_list* t_decl_node(ctron_list* t_file, const char* t_tag, const char* t_name) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)(t_tag)) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)(t_name)) == 0))) {
{
return t_d;
}
}
t_i += 1;
}
}
return ctron_list_new();
}
int t_send_of(ctron_list* t_file, ctron_list* t_ty, int32_t t_depth) 
{
if ((t_depth > 16)) {
{
return 1;
}
}
if ((((ctron_list*)(t_ty))->n == 0)) {
{
return 1;
}
}
const char* t_t = ((const char*)((ctron_list*)(t_ty))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Named")) == 0)) {
{
const char* t_n = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (t_s_prim(t_n)) {
{
return 1;
}
}
if (t_s_special(t_n)) {
{
return 1;
}
}
ctron_list* t_c = (ctron_list*)(t_decl_node(t_file, "Class", t_n));
if ((((ctron_list*)(t_c))->n > 0)) {
{
const char* t_items = ((const char*)((ctron_list*)(t_c))->items[3]);
int32_t t_j = 1;
{
const char* t_it = 0;
while ((t_j < ctron_len((const void*)(t_items)))) {
t_it = ((const char*)((ctron_list*)(t_items))->items[t_j]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_it))->items[0])), (const char*)("Field")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_it))->items[1])), (const char*)("true")) == 0)) {
{
return 0;
}
}
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_it))->items[3])), (t_depth + 1)))) {
{
return 0;
}
}
}
}
t_j += 1;
}
}
return 1;
}
}
ctron_list* t_st = (ctron_list*)(t_decl_node(t_file, "Struct", t_n));
if ((((ctron_list*)(t_st))->n > 0)) {
{
const char* t_fs = ((const char*)((ctron_list*)(t_st))->items[4]);
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(t_fs)))) {
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_fs))->items[t_k])))->items[3])), (t_depth + 1)))) {
{
return 0;
}
}
t_k += 1;
}
return 1;
}
}
ctron_list* t_en = (ctron_list*)(t_decl_node(t_file, "Enum", t_n));
if ((((ctron_list*)(t_en))->n > 0)) {
{
const char* t_vs = ((const char*)((ctron_list*)(t_en))->items[3]);
int32_t t_m = 1;
{
const char* t_v = 0;
const char* t_kind = 0;
while ((t_m < ctron_len((const void*)(t_vs)))) {
t_v = ((const char*)((ctron_list*)(t_vs))->items[t_m]);
t_kind = ((const char*)((ctron_list*)(t_v))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_kind))->items[0])), (const char*)("KTuple")) == 0)) {
{
int32_t t_q = 1;
while ((t_q < ctron_len((const void*)(t_kind)))) {
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_kind))->items[t_q])), (t_depth + 1)))) {
{
return 0;
}
}
t_q += 1;
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_kind))->items[0])), (const char*)("KStruct")) == 0)) {
{
int32_t t_q2 = 1;
while ((t_q2 < ctron_len((const void*)(t_kind)))) {
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_kind))->items[t_q2])))->items[3])), (t_depth + 1)))) {
{
return 0;
}
}
t_q2 += 1;
}
}
}
t_m += 1;
}
}
return 1;
}
}
return 1;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ref")) == 0)) {
{
const char* t_sub = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (((ctron_len((const void*)(t_sub)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("Named")) == 0))) {
{
const char* t_n2 = ((const char*)((ctron_list*)(t_sub))->items[1]);
ctron_list* t_tr = (ctron_list*)(t_decl_node(t_file, "Trait", t_n2));
if ((((ctron_list*)(t_tr))->n > 0)) {
{
return 0;
}
}
}
}
if (((ctron_len((const void*)(t_sub)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("Slice")) == 0))) {
{
return t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_sub))->items[1])), (t_depth + 1));
}
}
return t_send_of(t_file, (ctron_list*)(t_sub), (t_depth + 1));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Slice")) == 0)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("ArrayT")) == 0)) {
{
return t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])), (t_depth + 1));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Optional")) == 0)) {
{
return t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])), (t_depth + 1));
}
}
if ((strcmp((const char*)(t_t), (const char*)("TupleT")) == 0)) {
{
int32_t t_z = 1;
while ((t_z < ((ctron_list*)(t_ty))->n)) {
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[t_z])), (t_depth + 1)))) {
{
return 0;
}
}
t_z += 1;
}
return 1;
}
}
return 1;
}
const char* t_env_lookup(ctron_list* t_envN, ctron_list* t_envC, const char* t_nm) 
{
int32_t t_i = (((ctron_list*)(t_envN))->n - 1);
while ((t_i >= 0)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_envN))->items[t_i])), (const char*)(t_nm)) == 0)) {
{
return ((const char*)((ctron_list*)(t_envC))->items[t_i]);
}
}
t_i -= 1;
}
return "";
}
const char* t_ch_of_type(ctron_list* t_file, ctron_list* t_ty) 
{
if ((((ctron_list*)(t_ty))->n == 0)) {
{
return "";
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Named")) == 0)) {
{
const char* t_n = ((const char*)((ctron_list*)(t_ty))->items[1]);
ctron_list* t_c = (ctron_list*)(t_decl_node(t_file, "Class", t_n));
if ((((ctron_list*)(t_c))->n > 0)) {
{
return t_n;
}
}
}
}
return "";
}
void t_walk6e(ctron_list* t_file, ctron_list* t_e, int t_ow, ctron_list* t_envN, ctron_list* t_envC, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), 1, t_envN, t_envC, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_envN, t_envC, t_diags);
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_envN, t_envC, t_diags);
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_envN, t_envC, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_envN, t_envC, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_envN, t_envC, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i2 = 1;
while ((t_i2 < ctron_len((const void*)(t_arms)))) {
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i2])))->items[2])), t_ow, t_envN, t_envC, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_envN, t_envC, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_envN, t_envC, t_diags);
return;
}
}
}
void t_walk6b(ctron_list* t_file, ctron_list* t_b, int t_ow, ctron_list* t_envN, ctron_list* t_envC, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_ch = "";
const char* t_ty0 = ((const char*)((ctron_list*)(t_st))->items[3]);
t_ch = t_ch_of_type(t_file, (ctron_list*)(t_ty0));
if ((strcmp((const char*)(t_ch), (const char*)("")) == 0)) {
{
const char* t_ex = ((const char*)((ctron_list*)(t_st))->items[4]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ex))->items[0])), (const char*)("StructLit")) == 0)) {
{
const char* t_cn = ((const char*)((ctron_list*)(t_ex))->items[1]);
if ((((ctron_list*)(t_decl_node(t_file, "Class", t_cn)))->n > 0)) {
{
t_ch = t_cn;
}
}
}
}
}
}
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0) && (strcmp((const char*)(t_ch), (const char*)("")) != 0))) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_pat))->items[1]));
ctron_list_push((ctron_list*)(t_envC), t_ch);
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_pat))->items[1]));
ctron_list_push((ctron_list*)(t_envC), "");
}
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
if (t_ow) {
{
const char* t_tgt = ((const char*)((ctron_list*)(t_st))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tgt))->items[0])), (const char*)("Member")) == 0)) {
{
const char* t_obj = ((const char*)((ctron_list*)(t_tgt))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_obj))->items[0])), (const char*)("Ident")) == 0)) {
{
const char* t_nm = ((const char*)((ctron_list*)(t_obj))->items[1]);
const char* t_ch2 = t_env_lookup(t_envN, t_envC, t_nm);
if ((strcmp((const char*)(t_ch2), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E3060@"), (const char*)(t_nline((ctron_list*)(t_st))))), (const char*)(": own 块内对 GC 值可变写(mutable):"))), (const char*)(t_nm)));
}
}
}
}
}
}
}
}
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ow, t_envN, t_envC, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_envN, t_envC, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_envN, t_envC, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_envN, t_envC, t_diags);
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ow, t_envN, t_envC, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ow, t_envN, t_envC, t_diags);
t_walk6b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ow, t_envN, t_envC, t_diags);
}
}
}
}
}
}
}
t_i += 1;
}
}
t_walk6e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_ow, t_envN, t_envC, t_diags);
}
const char* t_sem_e3060(ctron_list* t_file, ctron_list* t_d, const char* t_tag) 
{
const char* t_out = "";
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_envN = (ctron_list*)(ctron_list_new());
ctron_list* t_envC = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[2]);
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
}
}
ctron_list* t_ps = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_ps = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]));
}
}
int32_t t_k = 1;
{
const char* t_p = 0;
while ((t_k < ((ctron_list*)(t_ps))->n)) {
t_p = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Param")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_p))->items[2]));
ctron_list_push((ctron_list*)(t_envC), t_ch_of_type(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_p))->items[3]))));
}
}
t_k += 1;
}
}
t_walk6b(t_file, (ctron_list*)(t_body), 0, t_envN, t_envC, t_diags);
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
int t_is_trait(ctron_list* t_file, const char* t_nm) 
{
return (((ctron_list*)(t_decl_node(t_file, "Trait", t_nm)))->n > 0);
}
ctron_list* t_env_ty(ctron_list* t_envN, ctron_list* t_envT, const char* t_nm) 
{
int32_t t_i = (((ctron_list*)(t_envN))->n - 1);
while ((t_i >= 0)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_envN))->items[t_i])), (const char*)(t_nm)) == 0)) {
{
return ((const char*)((ctron_list*)(t_envT))->items[t_i]);
}
}
t_i -= 1;
}
return ctron_list_new();
}
int t_cap_ty(ctron_list* t_file, ctron_list* t_ty) 
{
if (((((ctron_list*)(t_ty))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Ref")) == 0))) {
{
const char* t_sub = ((const char*)((ctron_list*)(t_ty))->items[1]);
if ((((ctron_len((const void*)(t_sub)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("Named")) == 0)) && t_is_trait(t_file, ((const char*)((ctron_list*)(t_sub))->items[1])))) {
{
return 1;
}
}
}
}
return 0;
}
void t_pwalk_e(ctron_list* t_file, ctron_list* t_e, const char* t_code, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) {
{
const char* t_obj = ((const char*)((ctron_list*)(t_cal))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_obj))->items[0])), (const char*)("Ident")) == 0)) {
{
ctron_list* t_ty = (ctron_list*)(t_env_ty(t_envN, t_envT, ((const char*)((ctron_list*)(t_obj))->items[1])));
if (t_cap_ty(t_file, t_ty)) {
{
ctron_list_push((ctron_list*)(t_diags), t_diag_render0(t_code, t_nline(t_e)));
}
}
}
}
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])), t_code, t_envN, t_envT, t_diags);
}
}
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_code, t_envN, t_envT, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_code, t_envN, t_envT, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_code, t_envN, t_envT, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_code, t_envN, t_envT, t_diags);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_code, t_envN, t_envT, t_diags);
return;
}
}
}
void t_pwalk_b(ctron_list* t_file, ctron_list* t_b, const char* t_code, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_code, t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_code, t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_code, t_envN, t_envT, t_diags);
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_code, t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_code, t_envN, t_envT, t_diags);
t_pwalk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_code, t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_ex = ((const char*)((ctron_list*)(t_st))->items[4]);
if (((ctron_len((const void*)(t_ex)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_ex))->items[0])), (const char*)("Closure")) == 0))) {
{
t_pwalk_e(t_file, (ctron_list*)(t_ex), t_code, t_envN, t_envT, t_diags);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
t_pwalk_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_code, t_envN, t_envT, t_diags);
}
const char* t_sem_cap(ctron_list* t_file, ctron_list* t_d, const char* t_kind) 
{
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_envN = (ctron_list*)(ctron_list_new());
ctron_list* t_envT = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_k = 1;
{
const char* t_p = 0;
while ((t_k < ctron_len((const void*)(t_ps)))) {
t_p = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Param")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_p))->items[2]));
ctron_list_push((ctron_list*)(t_envT), ((const char*)((ctron_list*)(t_p))->items[3]));
}
}
t_k += 1;
}
}
t_pwalk_b(t_file, (ctron_list*)(t_body), t_kind, t_envN, t_envT, t_diags);
const char* t_out = "";
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
void t_cle(ctron_list* t_file, ctron_list* t_e, ctron_list* t_names) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
ctron_list_push((ctron_list*)(t_names), ((const char*)((ctron_list*)(t_e))->items[1]));
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_names);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_names);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_names);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_names);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_names);
return;
}
}
}
void t_clb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_names) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_names);
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_names);
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_names);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_names);
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_names);
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])))->items[0])), (const char*)("None")) != 0)) {
{
t_clb(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])))->items[2])), t_names);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_names);
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_names);
}
int t_send_env(ctron_list* t_file, const char* t_nm, ctron_list* t_envN, ctron_list* t_envT) 
{
int32_t t_i = (((ctron_list*)(t_envN))->n - 1);
while ((t_i >= 0)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_envN))->items[t_i])), (const char*)(t_nm)) == 0)) {
{
return t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_envT))->items[t_i])), 0);
}
}
t_i -= 1;
}
return 1;
}
ctron_list* t_let_ty(ctron_list* t_file, ctron_list* t_st) 
{
const char* t_ann = ((const char*)((ctron_list*)(t_st))->items[3]);
if (((ctron_len((const void*)(t_ann)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_ann))->items[0])), (const char*)("None")) != 0))) {
{
return t_ann;
}
}
const char* t_ex = ((const char*)((ctron_list*)(t_st))->items[4]);
if (((ctron_len((const void*)(t_ex)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_ex))->items[0])), (const char*)("StructLit")) == 0))) {
{
const char* t_cn = ((const char*)((ctron_list*)(t_ex))->items[1]);
if ((((ctron_list*)(t_decl_node(t_file, "Class", t_cn)))->n > 0)) {
{
ctron_list* t_nt = (ctron_list*)(t_mk("Named"));
ctron_list_push((ctron_list*)(t_nt), t_cn);
return t_nt;
}
}
}
}
return ctron_list_new();
}
void t_wse(ctron_list* t_file, ctron_list* t_e, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[3])), (const char*)("spawn")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) > 1))) {
{
const char* t_arg0 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_arg0))->items[0])), (const char*)("Closure")) == 0)) {
{
ctron_list* t_caps = (ctron_list*)(ctron_list_new());
t_cle(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_arg0))->items[3])), t_caps);
int32_t t_ci = 0;
int t_done = 0;
{
const char* t_nm = 0;
while (((t_ci < ((ctron_list*)(t_caps))->n) && (!t_done))) {
t_nm = ((const char*)((ctron_list*)(t_caps))->items[t_ci]);
if ((!t_send_env(t_file, t_nm, t_envN, t_envT))) {
{
t_diag1(t_diags, "E3010", t_nline(t_e), t_nm);
t_done = 1;
}
}
t_ci += 1;
}
}
}
}
}
}
t_wse(t_file, (ctron_list*)(t_cal), t_envN, t_envT, t_diags);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_envN, t_envT, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_envN, t_envT, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_envN, t_envT, t_diags);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
}
void t_wsb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
ctron_list* t_ty = (ctron_list*)(t_let_ty(t_file, (ctron_list*)(t_st)));
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_pat))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_ty));
}
}
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_envN, t_envT, t_diags);
t_wsb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_envN, t_envT, t_diags);
}
}
}
}
}
}
}
t_i += 1;
}
}
t_wse(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_envN, t_envT, t_diags);
}
const char* t_sem_spawn2(ctron_list* t_file, ctron_list* t_d, const char* t_tag) 
{
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_envN = (ctron_list*)(ctron_list_new());
ctron_list* t_envT = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[2]);
ctron_list* t_ps = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
t_ps = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]));
}
}
int32_t t_k = 1;
{
const char* t_p = 0;
while ((t_k < ((ctron_list*)(t_ps))->n)) {
t_p = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Param")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_p))->items[2]));
ctron_list_push((ctron_list*)(t_envT), ((const char*)((ctron_list*)(t_p))->items[3]));
}
}
t_k += 1;
}
}
t_wsb(t_file, (ctron_list*)(t_body), t_envN, t_envT, t_diags);
const char* t_out = "";
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
int t_in_list(ctron_list* t_ls, const char* t_nm) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_ls))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ls))->items[t_i])), (const char*)(t_nm)) == 0)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
ctron_list* t_rm_name(ctron_list* t_ls, const char* t_nm) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_ls))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ls))->items[t_i])), (const char*)(t_nm)) != 0)) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_ls))->items[t_i]));
}
}
t_i += 1;
}
return t_out;
}
int t_call_root_arena(ctron_list* t_e) 
{
if (((((ctron_list*)(t_e))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_e))->items[0])), (const char*)("Call")) == 0))) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
while (((ctron_len((const void*)(t_cal)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0))) {
t_cal = ((const char*)((ctron_list*)(t_cal))->items[1]);
}
if ((((((ctron_len((const void*)(t_cal)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_cal))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[0])), (const char*)("Ident")) == 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1])), (const char*)("arena")) == 0))) {
{
return 1;
}
}
}
}
return 0;
}
void t_me(ctron_list* t_file, ctron_list* t_e, ctron_list* t_moved, ctron_list* t_arenaB, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
if (t_in_list(t_moved, ((const char*)((ctron_list*)(t_e))->items[1]))) {
{
t_diag1(t_diags, "E3050.umove", t_nline(t_e), ((const char*)((ctron_list*)(t_e))->items[1]));
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_moved, t_arenaB, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_moved, t_arenaB, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_moved, t_arenaB, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_moved, t_arenaB, t_diags);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
if (((((ctron_list*)(t_e))->n > 1) && (!t_in_list(t_arenaB, ((const char*)((ctron_list*)(t_e))->items[1]))))) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_e))->items[1]));
}
}
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_moved, t_arenaB, t_diags);
return;
}
}
}
void t_mb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_moved, ctron_list* t_arenaB, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_moved, t_arenaB, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_moved, t_arenaB, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_moved, t_arenaB, t_diags);
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_moved, t_arenaB, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_moved, t_arenaB, t_diags);
t_mb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_moved, t_arenaB, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
const char* t_init = ((const char*)((ctron_list*)(t_st))->items[4]);
int t_handled = 0;
if (((ctron_len((const void*)(t_init)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_init))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_src = ((const char*)((ctron_list*)(t_init))->items[1]);
if (t_in_list(t_arenaB, t_src)) {
{
ctron_list_push((ctron_list*)(t_moved), t_src);
t_arenaB = (ctron_list*)(t_rm_name(t_arenaB, t_src));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
t_handled = 1;
}
}
else {
if (t_in_list(t_moved, t_src)) {
{
t_diag1(t_diags, "E3050.umove", "", t_src);
t_handled = 1;
}
}
}
}
}
if ((!t_handled)) {
{
if ((((ctron_len((const void*)(t_init)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_init))->items[0])), (const char*)("Call")) == 0)) && t_call_root_arena((ctron_list*)(t_init)))) {
{
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0) && (!t_in_list(t_arenaB, ((const char*)((ctron_list*)(t_pat))->items[1]))))) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
}
}
t_me(t_file, (ctron_list*)(t_init), t_moved, t_arenaB, t_diags);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_moved, t_arenaB, t_diags);
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_moved, t_arenaB, t_diags);
}
}
}
}
}
}
}
t_i += 1;
}
}
t_me(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_moved, t_arenaB, t_diags);
}
const char* t_sem_mv(ctron_list* t_file, ctron_list* t_d, const char* t_tag) 
{
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_moved = (ctron_list*)(ctron_list_new());
ctron_list* t_arenaB = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[2]);
ctron_list* t_ps = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
t_ps = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]));
}
}
t_mb(t_file, (ctron_list*)(t_body), t_moved, t_arenaB, t_diags);
const char* t_out = "";
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
int t_seq(const char* t_a, const char* t_b) 
{
if ((strlen((const char*)(t_a)) != strlen((const char*)(t_b)))) {
{
return 0;
}
}
int32_t t_j = 0;
while ((t_j < strlen((const char*)(t_a)))) {
if ((ctron_byte_at(t_a, t_j) != ctron_byte_at(t_b, t_j))) {
{
return 0;
}
}
t_j += 1;
}
return 1;
}
ctron_list* t_variants_of(ctron_list* t_file, ctron_list* t_ty) 
{
ctron_list* t_vs = (ctron_list*)(ctron_list_new());
if ((((ctron_list*)(t_ty))->n == 0)) {
{
return t_vs;
}
}
const char* t_t = ((const char*)((ctron_list*)(t_ty))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Optional")) == 0)) {
{
ctron_list_push((ctron_list*)(t_vs), "Some");
ctron_list_push((ctron_list*)(t_vs), "None");
return t_vs;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Named")) == 0)) {
{
const char* t_n = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (t_or2((strcmp((const char*)(t_n), (const char*)("Option")) == 0), (strcmp((const char*)(t_n), (const char*)("Result")) == 0))) {
{
if ((strcmp((const char*)(t_n), (const char*)("Option")) == 0)) {
{
ctron_list_push((ctron_list*)(t_vs), "Some");
ctron_list_push((ctron_list*)(t_vs), "None");
}
}
else {
{
ctron_list_push((ctron_list*)(t_vs), "Ok");
ctron_list_push((ctron_list*)(t_vs), "Err");
}
}
return t_vs;
}
}
ctron_list* t_en = (ctron_list*)(t_decl_node(t_file, "Enum", t_n));
if ((((ctron_list*)(t_en))->n > 0)) {
{
const char* t_vars = ((const char*)((ctron_list*)(t_en))->items[3]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_vars)))) {
ctron_list_push((ctron_list*)(t_vs), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vars))->items[t_i])))->items[1]));
t_i += 1;
}
}
}
}
}
return t_vs;
}
ctron_list* t_ex_resolve(ctron_list* t_file, ctron_list* t_scrut, ctron_list* t_envN, ctron_list* t_envT) 
{
if (((((ctron_list*)(t_scrut))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_scrut))->items[0])), (const char*)("Ident")) == 0))) {
{
int32_t t_i = (((ctron_list*)(t_envN))->n - 1);
while ((t_i >= 0)) {
if (t_seq(((const char*)((ctron_list*)(t_envN))->items[t_i]), ((const char*)((ctron_list*)(t_scrut))->items[1]))) {
{
return ((const char*)((ctron_list*)(t_envT))->items[t_i]);
}
}
t_i -= 1;
}
}
}
return ctron_list_new();
}
void t_xe(ctron_list* t_file, ctron_list* t_e, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
ctron_list* t_ty = (ctron_list*)(t_ex_resolve(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT));
ctron_list* t_vs = (ctron_list*)(t_variants_of(t_file, t_ty));
if ((((ctron_list*)(t_vs))->n > 0)) {
{
ctron_list* t_covered = (ctron_list*)(ctron_list_new());
int t_whole = 0;
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_ai = 1;
{
const char* t_arm = 0;
const char* t_pat = 0;
while (((t_ai < ctron_len((const void*)(t_arms))) && (!t_whole))) {
t_arm = ((const char*)((ctron_list*)(t_arms))->items[t_ai]);
t_pat = ((const char*)((ctron_list*)(t_arm))->items[1]);
if ((ctron_len((const void*)(t_arm)) <= 3)) {
{
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatWild")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0))) {
{
t_whole = 1;
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatAgg")) == 0)) {
{
ctron_list_push((ctron_list*)(t_covered), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatOr")) == 0)) {
{
int t_oall = 1;
int32_t t_ok9 = 1;
{
const char* t_alt = 0;
while ((t_ok9 < ctron_len((const void*)(t_pat)))) {
t_alt = ((const char*)((ctron_list*)(t_pat))->items[t_ok9]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_alt))->items[0])), (const char*)("PatWild")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_alt))->items[0])), (const char*)("PatId")) == 0))) {
{
t_whole = 1;
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_alt))->items[0])), (const char*)("PatAgg")) == 0)) {
{
ctron_list_push((ctron_list*)(t_covered), ((const char*)((ctron_list*)(t_alt))->items[1]));
}
}
else {
{
t_oall = 0;
}
}
}
t_ok9 += 1;
}
}
}
}
}
}
}
}
t_ai += 1;
}
}
if ((!t_whole)) {
{
ctron_list* t_miss = (ctron_list*)(ctron_list_new());
int32_t t_vi = 0;
{
int t_hit = 0;
while ((t_vi < ((ctron_list*)(t_vs))->n)) {
t_hit = 0;
int32_t t_ci = 0;
while ((t_ci < ((ctron_list*)(t_covered))->n)) {
if (t_seq(((const char*)((ctron_list*)(t_covered))->items[t_ci]), ((const char*)((ctron_list*)(t_vs))->items[t_vi]))) {
{
t_hit = 1;
}
}
t_ci += 1;
}
if ((!t_hit)) {
{
ctron_list_push((ctron_list*)(t_miss), ((const char*)((ctron_list*)(t_vs))->items[t_vi]));
}
}
t_vi += 1;
}
}
if ((((ctron_list*)(t_miss))->n > 0)) {
{
const char* t_buf = "";
int32_t t_mi = 0;
while ((t_mi < ((ctron_list*)(t_miss))->n)) {
if ((t_mi > 0)) {
{
t_buf = ctron_str_concat((const char*)(t_buf), (const char*)(" "));
}
}
t_buf = ctron_str_concat((const char*)(t_buf), (const char*)(((const char*)((ctron_list*)(t_miss))->items[t_mi])));
t_mi += 1;
}
t_diag1(t_diags, "E2030", t_nline(t_e), t_buf);
}
}
}
}
}
}
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
const char* t_arms2 = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_aj = 1;
while ((t_aj < ctron_len((const void*)(t_arms2)))) {
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms2))->items[t_aj])))->items[2])), t_envN, t_envT, t_diags);
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_arms2))->items[t_aj]))) > 3)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms2))->items[t_aj])))->items[3])), t_envN, t_envT, t_diags);
}
}
t_aj += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_envN, t_envT, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_envN, t_envT, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_envN, t_envT, t_diags);
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_envN, t_envT, t_diags);
return;
}
}
}
void t_xb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_envN, ctron_list* t_envT, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_ann = ((const char*)((ctron_list*)(t_st))->items[3]);
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
if (((ctron_len((const void*)(t_ann)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_ann))->items[0])), (const char*)("None")) != 0))) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_pat))->items[1]));
ctron_list_push((ctron_list*)(t_envT), t_ann);
}
}
else {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_pat))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
}
}
}
}
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_envN, t_envT, t_diags);
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_envN, t_envT, t_diags);
t_xb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_envN, t_envT, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_envN, t_envT, t_diags);
}
}
}
}
}
}
}
t_i += 1;
}
}
t_xe(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_envN, t_envT, t_diags);
}
const char* t_sem_exh(ctron_list* t_file, ctron_list* t_d, const char* t_tag) 
{
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_envN = (ctron_list*)(ctron_list_new());
ctron_list* t_envT = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[2]);
ctron_list* t_ps = (ctron_list*)(ctron_list_new());
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
t_ps = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]));
}
}
int32_t t_k = 1;
{
const char* t_p = 0;
while ((t_k < ((ctron_list*)(t_ps))->n)) {
t_p = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Param")) == 0)) {
{
ctron_list_push((ctron_list*)(t_envN), ((const char*)((ctron_list*)(t_p))->items[2]));
ctron_list_push((ctron_list*)(t_envT), ((const char*)((ctron_list*)(t_p))->items[3]));
}
}
t_k += 1;
}
}
t_xb(t_file, (ctron_list*)(t_body), t_envN, t_envT, t_diags);
const char* t_out = "";
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
int t_has_noalloc(ctron_list* t_node) 
{
return ((((ctron_list*)(t_node))->n >= 7) && (strcmp((const char*)(((const char*)((ctron_list*)(t_node))->items[6])), (const char*)("no_alloc")) == 0));
}
const char* t_mem_target(ctron_list* t_m) 
{
if ((((ctron_list*)(t_m))->n > 3)) {
{
return ((const char*)((ctron_list*)(t_m))->items[3]);
}
}
return "";
}
int t_fn_alloc_sum(ctron_list* t_file, const char* t_nm, ctron_list* t_vis) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_vis))->n)) {
if (t_seq(((const char*)((ctron_list*)(t_vis))->items[t_i]), t_nm)) {
{
return 1;
}
}
t_i += 1;
}
ctron_list* t_d = (ctron_list*)(t_decl_node(t_file, "Fn", t_nm));
if ((((ctron_list*)(t_d))->n == 0)) {
{
return 0;
}
}
if (t_has_noalloc(t_d)) {
{
return 0;
}
}
if (((((ctron_list*)(t_d))->n >= 7) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[6])), (const char*)("no_spawn")) == 0))) {
{
}
}
ctron_list_push((ctron_list*)(t_vis), t_nm);
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
int t_r = t_scan_alloc(t_file, (ctron_list*)(t_body), t_vis);
return t_r;
}
int t_scan_alloc(ctron_list* t_file, ctron_list* t_b, ctron_list* t_vis) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_vis)) {
{
return 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_vis)) {
{
return 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_vis)) {
{
return 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_vis)) {
{
return 1;
}
}
if (t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_vis)) {
{
return 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_vis)) {
{
return 1;
}
}
if (t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_vis)) {
{
return 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_vis)) {
{
return 1;
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_vis)) {
{
return 1;
}
}
return 0;
}
int t_sc_e(ctron_list* t_file, ctron_list* t_e, ctron_list* t_vis) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
if (t_call_allocish(t_file, t_e, t_vis)) {
{
return 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis)) {
{
return 1;
}
}
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_vis)) {
{
return 1;
}
}
t_i += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
return t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis)) {
{
return 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis)) {
{
return 1;
}
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis)) {
{
return 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vis)) {
{
return 1;
}
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis)) {
{
return 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vis)) {
{
return 1;
}
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
return t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
return t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis);
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_vis)) {
{
return 1;
}
}
t_i2 += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_cn = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((((ctron_list*)(t_decl_node(t_file, "Class", t_cn)))->n > 0)) {
{
return 1;
}
}
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_j = 1;
while ((t_j < ctron_len((const void*)(t_lf)))) {
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_lf))->items[t_j])))->items[2])), t_vis)) {
{
return 1;
}
}
t_j += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
return t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis)) {
{
return 1;
}
}
if (t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis)) {
{
return 1;
}
}
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vis)) {
{
return 1;
}
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_vis)) {
{
return 1;
}
}
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
if (t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_vis)) {
{
return 1;
}
}
t_i3 += 1;
}
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
return t_sc_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
return t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
return t_scan_alloc(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_vis);
}
}
return 0;
}
int t_call_allocish(ctron_list* t_file, ctron_list* t_e, ctron_list* t_vis) 
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) {
{
const char* t_m = t_mem_target((ctron_list*)(t_cal));
if (t_seq(t_m, "to_string")) {
{
return 1;
}
}
if (t_seq(t_m, "into_gc")) {
{
return 1;
}
}
if (t_seq(t_m, "push")) {
{
return 1;
}
}
if (t_or3(t_seq(t_m, "list"), t_seq(t_m, "array"), t_seq(t_m, "zeros"))) {
{
const char* t_obj = ((const char*)((ctron_list*)(t_cal))->items[1]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_obj))->items[0])), (const char*)("Ident")) == 0) && t_seq(((const char*)((ctron_list*)(t_obj))->items[1]), "arena"))) {
{
return 0;
}
}
return 0;
}
}
return 0;
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_n = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1]);
if (t_or2(t_or2(t_seq(t_n, "Box"), t_seq(t_n, "List")), t_or2(t_seq(t_n, "Map"), t_seq(t_n, "String")))) {
{
return 1;
}
}
return 0;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
return t_fn_alloc_sum(t_file, ((const char*)((ctron_list*)(t_cal))->items[1]), t_vis);
}
}
return 0;
}
const char* t_member_root(ctron_list* t_m) 
{
ctron_list* t_c = (ctron_list*)(t_m);
while (((((((ctron_list*)(t_c))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_c))->items[0])), (const char*)("Member")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_c))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[1])))->items[0])), (const char*)("Member")) == 0))) {
t_c = (ctron_list*)(((const char*)((ctron_list*)(t_c))->items[1]));
}
if (((((((ctron_list*)(t_c))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_c))->items[0])), (const char*)("Member")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_c))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[1])))->items[0])), (const char*)("Ident")) == 0))) {
{
return ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[1])))->items[1]);
}
}
if (((((ctron_list*)(t_c))->n > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_c))->items[0])), (const char*)("Ident")) == 0))) {
{
return ((const char*)((ctron_list*)(t_c))->items[1]);
}
}
return "";
}
void t_al_e(ctron_list* t_file, ctron_list* t_e, int t_ow, int t_na, ctron_list* t_arenaB, ctron_list* t_gcL, ctron_list* t_diags) 
{
int t_restricted = t_or2(t_ow, t_na);
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if (((strcmp((const char*)(t_t), (const char*)("Call")) == 0) && t_restricted)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
int t_alloc = 0;
const char* t_kind = "调用";
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) {
{
const char* t_m = t_mem_target((ctron_list*)(t_cal));
if (t_seq(t_m, "to_string")) {
{
t_alloc = 1;
t_kind = t_member_root((ctron_list*)(t_cal));
}
}
else {
if (t_seq(t_m, "into_gc")) {
{
t_alloc = (!t_ow);
t_kind = t_member_root((ctron_list*)(t_cal));
}
}
else {
if (t_seq(t_m, "push")) {
{
if (t_ow) {
{
const char* t_obj = ((const char*)((ctron_list*)(t_cal))->items[1]);
int t_allow = 0;
if (((ctron_len((const void*)(t_obj)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_obj))->items[0])), (const char*)("Ident")) == 0))) {
{
if (t_in_list(t_arenaB, ((const char*)((ctron_list*)(t_obj))->items[1]))) {
{
t_allow = 1;
}
}
if (t_in_list(t_gcL, ((const char*)((ctron_list*)(t_obj))->items[1]))) {
{
t_allow = 1;
}
}
}
}
if ((!t_allow)) {
{
t_alloc = 1;
}
}
}
}
else {
{
t_alloc = 1;
}
}
t_kind = t_member_root((ctron_list*)(t_cal));
}
}
}
}
}
}
else {
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_cal))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_n = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1]);
if (t_or2(t_or2(t_seq(t_n, "Box"), t_seq(t_n, "List")), t_or2(t_seq(t_n, "Map"), t_seq(t_n, "String")))) {
{
t_alloc = 1;
t_kind = t_n;
}
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
if (t_fn_alloc_sum(t_file, ((const char*)((ctron_list*)(t_cal))->items[1]), ctron_list_new())) {
{
t_alloc = 1;
t_kind = ((const char*)((ctron_list*)(t_cal))->items[1]);
}
}
}
}
}
}
if (t_alloc) {
{
if (t_ow) {
{
t_diag1(t_diags, "E3040.own", t_nline(t_e), t_kind);
}
}
else {
{
t_diag1(t_diags, "E3040.noalloc", t_nline(t_e), t_kind);
}
}
return;
}
}
}
}
if (((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0) && t_restricted)) {
{
const char* t_cn = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((((ctron_list*)(t_decl_node(t_file, "Class", t_cn)))->n > 0)) {
{
if (t_ow) {
{
t_diag1(t_diags, "E3040.own", t_nline(t_e), "类构造");
}
}
else {
{
t_diag1(t_diags, "E3040.noalloc", t_nline(t_e), "类构造");
}
}
return;
}
}
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_j = 1;
while ((t_j < ctron_len((const void*)(t_lf)))) {
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_lf))->items[t_j])))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_j += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
if (((((ctron_list*)(t_e))->n > 1) && (!t_in_list(t_arenaB, ((const char*)((ctron_list*)(t_e))->items[1]))))) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_e))->items[1]));
}
}
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), 1, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
return;
}
}
}
void t_al_b(ctron_list* t_file, ctron_list* t_b, int t_ow, int t_na, ctron_list* t_arenaB, ctron_list* t_gcL, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < (((ctron_list*)(t_b))->n - 1))) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
const char* t_init = ((const char*)((ctron_list*)(t_st))->items[4]);
if (t_ow) {
{
int t_handled = 0;
if (((ctron_len((const void*)(t_init)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_init))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_src = ((const char*)((ctron_list*)(t_init))->items[1]);
if (t_in_list(t_arenaB, t_src)) {
{
t_arenaB = (ctron_list*)(t_rm_name(t_arenaB, t_src));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
t_handled = 1;
}
}
}
}
if ((((!t_handled) && (ctron_len((const void*)(t_init)) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(t_init))->items[0])), (const char*)("Call")) == 0))) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_init))->items[1]);
if ((((ctron_len((const void*)(t_cal)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) && t_seq(t_mem_target((ctron_list*)(t_cal)), "into_gc"))) {
{
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0) && (!t_in_list(t_gcL, ((const char*)((ctron_list*)(t_pat))->items[1]))))) {
{
ctron_list_push((ctron_list*)(t_gcL), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
}
}
else {
if ((((((((ctron_len((const void*)(t_cal)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_cal))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[0])), (const char*)("Member")) == 0)) && (ctron_len((const void*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1]))) > 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1])))->items[0])), (const char*)("Ident")) == 0)) && t_seq(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])))->items[1])))->items[1]), "arena"))) {
{
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0) && (!t_in_list(t_arenaB, ((const char*)((ctron_list*)(t_pat))->items[1]))))) {
{
ctron_list_push((ctron_list*)(t_arenaB), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
}
}
}
}
}
}
}
t_al_e(t_file, (ctron_list*)(t_init), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
}
}
}
}
}
}
t_i += 1;
}
}
t_al_e(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_b))->items[(((ctron_list*)(t_b))->n - 1)])), t_ow, t_na, t_arenaB, t_gcL, t_diags);
}
const char* t_sem_alloc(ctron_list* t_file, ctron_list* t_d, const char* t_tag, const char* t_prof) 
{
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_arenaB = (ctron_list*)(ctron_list_new());
ctron_list* t_gcL = (ctron_list*)(ctron_list_new());
const char* t_body = ((const char*)((ctron_list*)(t_d))->items[2]);
int t_na = 0;
if (t_seq2(t_prof, "bare")) {
{
t_na = 1;
}
}
if ((strcmp((const char*)(t_tag), (const char*)("Test")) != 0)) {
{
t_body = ((const char*)((ctron_list*)(t_d))->items[5]);
if (t_has_noalloc(t_d)) {
{
t_na = 1;
}
}
}
}
t_al_b(t_file, (ctron_list*)(t_body), 0, t_na, t_arenaB, t_gcL, t_diags);
const char* t_out = "";
int32_t t_q = 0;
while ((t_q < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_q])))), (const char*)("\n"));
t_q += 1;
}
return t_out;
}
int t_trait_method_noalloc(ctron_list* t_file, const char* t_tr, const char* t_mnm) 
{
ctron_list* t_td = (ctron_list*)(t_decl_node(t_file, "Trait", t_tr));
if ((((ctron_list*)(t_td))->n > 0)) {
{
const char* t_items = ((const char*)((ctron_list*)(t_td))->items[4]);
int32_t t_i = 1;
{
const char* t_it = 0;
while ((t_i < ctron_len((const void*)(t_items)))) {
t_it = ((const char*)((ctron_list*)(t_items))->items[t_i]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_it))->items[0])), (const char*)("Method")) == 0) && t_seq(((const char*)((ctron_list*)(t_it))->items[1]), t_mnm)) && t_has_noalloc((ctron_list*)(t_it)))) {
{
return 1;
}
}
t_i += 1;
}
}
}
}
return 0;
}
const char* t_sem_walk2(ctron_list* t_file, const char* t_prof) 
{
const char* t_out = "";
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
ctron_list* t_cls = (ctron_list*)(ctron_list_new());
ctron_list* t_fns = (ctron_list*)(ctron_list_new());
ctron_list* t_rets = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_d))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Class")) == 0)) {
{
ctron_list_push((ctron_list*)(t_cls), ((const char*)((ctron_list*)(t_d))->items[1]));
}
}
else {
if (t_or2((strcmp((const char*)(t_t), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnPub")) == 0), (strcmp((const char*)(t_t), (const char*)("FnExt")) == 0))))) {
{
ctron_list_push((ctron_list*)(t_fns), ((const char*)((ctron_list*)(t_d))->items[1]));
ctron_list_push((ctron_list*)(t_rets), ((const char*)((ctron_list*)(t_d))->items[4]));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Method")) == 0)) {
{
ctron_list_push((ctron_list*)(t_fns), ((const char*)((ctron_list*)(t_d))->items[1]));
ctron_list_push((ctron_list*)(t_rets), ((const char*)((ctron_list*)(t_d))->items[4]));
}
}
else {
if (((strcmp((const char*)(t_t), (const char*)("Static")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[2])), (const char*)("true")) == 0))) {
{
t_diag1(t_diags, "E3030", t_nline((ctron_list*)(t_d)), ((const char*)((ctron_list*)(t_d))->items[1]));
}
}
}
}
}
t_i += 1;
}
}
t_i = 1;
{
const char* t_d = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_d))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Struct")) == 0)) {
{
const char* t_nm = ((const char*)((ctron_list*)(t_d))->items[1]);
int t_hit = 0;
const char* t_hn = "";
const char* t_fs = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_j = 1;
{
const char* t_f = 0;
const char* t_ty = 0;
while (((t_j < ctron_len((const void*)(t_fs))) && (!t_hit))) {
t_f = ((const char*)((ctron_list*)(t_fs))->items[t_j]);
t_ty = ((const char*)((ctron_list*)(t_f))->items[3]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Named")) == 0)) {
{
const char* t_h = ((const char*)((ctron_list*)(t_ty))->items[1]);
int32_t t_k = 0;
while (((t_k < ((ctron_list*)(t_cls))->n) && (!t_hit))) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cls))->items[t_k])), (const char*)(t_h)) == 0)) {
{
t_hit = 1;
t_hn = ((const char*)((ctron_list*)(t_f))->items[2]);
}
}
t_k += 1;
}
}
}
t_j += 1;
}
}
if (t_hit) {
{
t_diag2(t_diags, "W8010", t_nline((ctron_list*)(t_d)), t_nm, t_hn);
}
}
}
}
else {
if (t_or2((strcmp((const char*)(t_t), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnC")) == 0), (strcmp((const char*)(t_t), (const char*)("FnPub")) == 0)))) {
{
int t_ns = 0;
if (((ctron_len((const void*)(t_d)) >= 7) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[6])), (const char*)("no_spawn")) == 0))) {
{
t_ns = 1;
}
}
const char* t_ps4 = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_pi4 = 1;
while ((t_pi4 < ctron_len((const void*)(t_ps4)))) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ps4))->items[t_pi4])))->items[0])), (const char*)("Param")) == 0)) {
{
const char* t_pn4 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ps4))->items[t_pi4])))->items[2]);
if (t_is_prelude_name(t_pn4)) {
{
ctron_list_push((ctron_list*)(t_diags), t_diag_render1("W8040", "0", t_pn4));
}
}
}
}
t_pi4 += 1;
}
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_ns, t_fns, t_rets, t_diags, 1);
const char* t_e6 = t_sem_e3060(t_file, (ctron_list*)(t_d), t_t);
if ((strcmp((const char*)(t_e6), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_e6, 0, (strlen((const char*)(t_e6)) - 1)));
}
}
const char* t_ckind = "";
if (((ctron_len((const void*)(t_d)) >= 7) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[6])), (const char*)("pure")) == 0))) {
{
t_ckind = "E4020.cap";
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("FnC")) == 0)) {
{
t_ckind = "E6020.cap";
}
}
}
if ((strcmp((const char*)(t_ckind), (const char*)("")) != 0)) {
{
const char* t_ec = t_sem_cap(t_file, (ctron_list*)(t_d), t_ckind);
if ((strcmp((const char*)(t_ec), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_ec, 0, (strlen((const char*)(t_ec)) - 1)));
}
}
}
}
if ((!((ctron_len((const void*)(t_d)) >= 7) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[6])), (const char*)("no_spawn")) == 0)))) {
{
const char* t_es = t_sem_spawn2(t_file, (ctron_list*)(t_d), t_t);
if ((strcmp((const char*)(t_es), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_es, 0, (strlen((const char*)(t_es)) - 1)));
}
}
}
}
const char* t_mv = t_sem_mv(t_file, (ctron_list*)(t_d), t_t);
if ((strcmp((const char*)(t_mv), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_mv, 0, (strlen((const char*)(t_mv)) - 1)));
}
}
const char* t_ex = t_sem_exh(t_file, (ctron_list*)(t_d), t_t);
if ((strcmp((const char*)(t_ex), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_ex, 0, (strlen((const char*)(t_ex)) - 1)));
}
}
const char* t_ac = t_sem_alloc(t_file, (ctron_list*)(t_d), t_t, t_prof);
if ((strcmp((const char*)(t_ac), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_ac, 0, (strlen((const char*)(t_ac)) - 1)));
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Test")) == 0)) {
{
t_walk_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), 0, t_fns, t_rets, t_diags, 1);
const char* t_e6b = t_sem_e3060(t_file, (ctron_list*)(t_d), "Test");
if ((strcmp((const char*)(t_e6b), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_e6b, 0, (strlen((const char*)(t_e6b)) - 1)));
}
}
const char* t_es2 = t_sem_spawn2(t_file, (ctron_list*)(t_d), "Test");
if ((strcmp((const char*)(t_es2), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_es2, 0, (strlen((const char*)(t_es2)) - 1)));
}
}
const char* t_mv2 = t_sem_mv(t_file, (ctron_list*)(t_d), "Test");
if ((strcmp((const char*)(t_mv2), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_mv2, 0, (strlen((const char*)(t_mv2)) - 1)));
}
}
const char* t_ex2 = t_sem_exh(t_file, (ctron_list*)(t_d), "Test");
if ((strcmp((const char*)(t_ex2), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_ex2, 0, (strlen((const char*)(t_ex2)) - 1)));
}
}
const char* t_ac2 = t_sem_alloc(t_file, (ctron_list*)(t_d), "Test", t_prof);
if ((strcmp((const char*)(t_ac2), (const char*)("")) != 0)) {
{
ctron_list_push((ctron_list*)(t_diags), ctron_byte_slice(t_ac2, 0, (strlen((const char*)(t_ac2)) - 1)));
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Static")) == 0)) {
{
if ((!t_send_of(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])), 0))) {
{
t_diag1(t_diags, "E3031", t_nline((ctron_list*)(t_d)), ((const char*)((ctron_list*)(t_d))->items[1]));
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Impl")) == 0)) {
{
const char* t_tr = ((const char*)((ctron_list*)(t_d))->items[2]);
const char* t_trname = "";
if (((ctron_len((const void*)(t_tr)) > 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_tr))->items[0])), (const char*)("Named")) == 0))) {
{
t_trname = ((const char*)((ctron_list*)(t_tr))->items[1]);
}
}
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_mi = 1;
{
const char* t_md = 0;
while ((t_mi < ctron_len((const void*)(t_its)))) {
t_md = ((const char*)((ctron_list*)(t_its))->items[t_mi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Method")) == 0)) {
{
int t_mna = t_or2(t_has_noalloc((ctron_list*)(t_md)), t_trait_method_noalloc(t_file, t_trname, ((const char*)((ctron_list*)(t_md))->items[1])));
if (t_mna) {
{
ctron_list* t_dl = (ctron_list*)(ctron_list_new());
ctron_list* t_ab = (ctron_list*)(ctron_list_new());
ctron_list* t_gl = (ctron_list*)(ctron_list_new());
t_al_b(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_md))->items[5])), 0, 1, t_ab, t_gl, t_dl);
int32_t t_qq = 0;
while ((t_qq < ((ctron_list*)(t_dl))->n)) {
ctron_list_push((ctron_list*)(t_diags), ((const char*)((ctron_list*)(t_dl))->items[t_qq]));
t_qq += 1;
}
}
}
}
}
t_mi += 1;
}
}
}
}
}
}
}
}
t_i += 1;
}
}
t_sem_calls_all(t_file, t_diags, t_prof);
int32_t t_ti = 1;
{
const char* t_td = 0;
while ((t_ti < ((ctron_list*)(t_file))->n)) {
t_td = ((const char*)((ctron_list*)(t_file))->items[t_ti]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("FnExt")) == 0)) {
{
if ((ctron_len((const void*)(t_td)) < 8)) {
{
t_diag1(t_diags, "W8050", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
if (((ctron_len((const void*)(t_td)) > 7) && t_attr_is(((const char*)((ctron_list*)(t_td))->items[6]), "repr"))) {
{
t_diag1(t_diags, "E4041", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
const char* t_eps = ((const char*)((ctron_list*)(t_td))->items[3]);
int32_t t_ek = 1;
{
const char* t_epr = 0;
while ((t_ek < ctron_len((const void*)(t_eps)))) {
t_epr = ((const char*)((ctron_list*)(t_eps))->items[t_ek]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_epr))->items[0])), (const char*)("Param")) == 0) && t_ext_nonabi_ty((ctron_list*)(((const char*)((ctron_list*)(t_epr))->items[3]))))) {
{
t_diag2(t_diags, "W8052.param", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]), ((const char*)((ctron_list*)(t_epr))->items[2]));
}
}
t_ek += 1;
}
}
if (t_ext_nonabi_ty((ctron_list*)(((const char*)((ctron_list*)(t_td))->items[4])))) {
{
t_diag1(t_diags, "W8052.ret", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
}
}
else {
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("Fn")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("FnPub")) == 0))) {
{
if (((ctron_len((const void*)(t_td)) > 6) && t_seq2(((const char*)((ctron_list*)(t_td))->items[6]), "trusted"))) {
{
t_diag1(t_diags, "E4040", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
else {
if (((ctron_len((const void*)(t_td)) > 6) && t_attr_is(((const char*)((ctron_list*)(t_td))->items[6]), "repr"))) {
{
t_diag1(t_diags, "E4041", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
}
const char* t_vps = ((const char*)((ctron_list*)(t_td))->items[3]);
int32_t t_vk = 1;
while ((t_vk < ctron_len((const void*)(t_vps)))) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vps))->items[t_vk])))->items[0])), (const char*)("VaArgs")) == 0)) {
{
const char* t_vln = "0";
if ((ctron_len((const void*)(t_td)) > 7)) {
{
t_vln = t_nline((ctron_list*)(t_td));
}
}
t_diag1(t_diags, "E4044", t_vln, ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
t_vk += 1;
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("Struct")) == 0)) {
{
if (((ctron_len((const void*)(t_td)) > 6) && t_attr_is(((const char*)((ctron_list*)(t_td))->items[5]), "repr"))) {
{
const char* t_rfs = ((const char*)((ctron_list*)(t_td))->items[4]);
int32_t t_rj = 1;
{
const char* t_rfd = 0;
while ((t_rj < ctron_len((const void*)(t_rfs)))) {
t_rfd = ((const char*)((ctron_list*)(t_rfs))->items[t_rj]);
if ((((ctron_len((const void*)(t_rfd)) > 3) && (strcmp((const char*)(((const char*)((ctron_list*)(t_rfd))->items[0])), (const char*)("Field")) == 0)) && t_ext_nonabi_ty((ctron_list*)(((const char*)((ctron_list*)(t_rfd))->items[3]))))) {
{
t_diag2(t_diags, "W8051", t_nline((ctron_list*)(t_td)), ((const char*)((ctron_list*)(t_td))->items[1]), ((const char*)((ctron_list*)(t_rfd))->items[2]));
}
}
t_rj += 1;
}
}
}
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("Enum")) == 0)) {
{
if (((ctron_len((const void*)(t_td)) > 4) && t_attr_is(((const char*)((ctron_list*)(t_td))->items[4]), "repr"))) {
{
t_diag1(t_diags, "E4041", "0", ((const char*)((ctron_list*)(t_td))->items[1]));
}
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_td))->items[0])), (const char*)("Class")) == 0)) {
{
const char* t_rfs = ((const char*)((ctron_list*)(t_td))->items[3]);
int32_t t_rj = 1;
{
const char* t_rfd = 0;
while ((t_rj < ctron_len((const void*)(t_rfs)))) {
t_rfd = ((const char*)((ctron_list*)(t_rfs))->items[t_rj]);
if (((ctron_len((const void*)(t_rfd)) > 3) && (strcmp((const char*)(((const char*)((ctron_list*)(t_rfd))->items[0])), (const char*)("Field")) == 0))) {
{
const char* t_rh = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_rfd))->items[3])));
if (t_or2(t_seq2(t_rh, "Mutex"), t_seq2(t_rh, "Channel"))) {
{
t_diag2(t_diags, "E4050", t_nline((ctron_list*)(t_rfd)), ((const char*)((ctron_list*)(t_td))->items[1]), ((const char*)((ctron_list*)(t_rfd))->items[2]));
}
}
}
}
t_rj += 1;
}
}
}
}
}
}
}
}
t_ti += 1;
}
}
t_sem_closures(t_file, t_diags);
t_sem_comp(t_file, t_diags);
int32_t t_kk = 0;
while ((t_kk < ((ctron_list*)(t_diags))->n)) {
t_out = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_kk])))), (const char*)("\n"));
t_kk += 1;
}
return t_out;
}
int t_attr_is(const char* t_v, const char* t_name) 
{
if (t_seq2(t_v, t_name)) {
{
return 1;
}
}
if ((strlen((const char*)(t_v)) > (strlen((const char*)(t_name)) + 1))) {
{
if ((ctron_byte_at(t_v, strlen((const char*)(t_name))) == 40)) {
{
return t_seq2(ctron_byte_slice(t_v, 0, strlen((const char*)(t_name))), t_name);
}
}
}
}
return 0;
}
int t_attr_has(const char* t_v, const char* t_name) 
{
int32_t t_i = 0;
const char* t_seg = "";
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_v)))) {
t_c = ctron_byte_at(t_v, t_i);
if ((t_c == 44)) {
{
if (t_attr_is(t_seg, t_name)) {
{
return 1;
}
}
t_seg = "";
}
}
else {
{
t_seg = ctron_str_concat((const char*)(t_seg), (const char*)(ctron_byte_slice(t_v, t_i, (t_i + 1))));
}
}
t_i += 1;
}
}
return t_attr_is(t_seg, t_name);
}
const char* t_attr_arg(const char* t_v) 
{
int32_t t_bar = (-1);
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_v)))) {
if ((ctron_byte_at(t_v, t_i) == 40)) {
{
t_bar = t_i;
t_i = strlen((const char*)(t_v));
}
}
t_i += 1;
}
if ((t_bar < 0)) {
{
return "";
}
}
int32_t t_dep = 1;
int32_t t_j = (t_bar + 1);
{
int32_t t_c = 0;
while ((t_j < strlen((const char*)(t_v)))) {
t_c = ctron_byte_at(t_v, t_j);
if ((t_c == 40)) {
{
t_dep += 1;
}
}
if ((t_c == 41)) {
{
t_dep -= 1;
if ((t_dep == 0)) {
{
return ctron_byte_slice(t_v, (t_bar + 1), t_j);
}
}
}
}
t_j += 1;
}
}
return "";
}
int t_ext_nonabi_ty(ctron_list* t_ty) 
{
const char* t_t = ((const char*)((ctron_list*)(t_ty))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Named")) == 0)) {
{
const char* t_h = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (t_seq2(t_h, "Option")) {
{
if (((((((ctron_list*)(t_ty))->n > 2) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_ty))->items[2]))) > 1)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[2])))->items[1])))->items[0])), (const char*)("Named")) == 0)) && t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[2])))->items[1])))->items[1]), "Str"))) {
{
return 0;
}
}
return 1;
}
}
if (t_or2(t_seq2(t_h, "List"), t_or2(t_seq2(t_h, "Atomic"), t_or2(t_seq2(t_h, "Result"), t_or2(t_seq2(t_h, "Box"), t_or2(t_seq2(t_h, "Mutex"), t_or2(t_seq2(t_h, "Channel"), t_seq2(t_h, "Global")))))))) {
{
return 1;
}
}
return 0;
}
}
if ((((ctron_list*)(t_ty))->n > 1)) {
{
return t_ext_nonabi_ty((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])));
}
}
return 0;
}
int t_seq2(const char* t_a, const char* t_b) 
{
if ((strlen((const char*)(t_a)) != strlen((const char*)(t_b)))) {
{
return 0;
}
}
int32_t t_j = 0;
while ((t_j < strlen((const char*)(t_a)))) {
if ((ctron_byte_at(t_a, t_j) != ctron_byte_at(t_b, t_j))) {
{
return 0;
}
}
t_j += 1;
}
return 1;
}
int t_prelude_ok(const char* t_nm) 
{
if (t_or2(t_seq2(t_nm, "print"), t_or2(t_seq2(t_nm, "println"), t_seq2(t_nm, "panic")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "assert"), t_or2(t_seq2(t_nm, "assert_eq"), t_seq2(t_nm, "assert_ne")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "read_file"), t_or2(t_seq2(t_nm, "read_dir"), t_or2(t_seq2(t_nm, "fs_exists"), t_or2(t_seq2(t_nm, "fs_write"), t_seq2(t_nm, "fs_delete")))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "byte_at"), t_or2(t_seq2(t_nm, "byte_slice"), t_seq2(t_nm, "utf8_enc")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "now_ms"), t_seq2(t_nm, "now_ms_text"))) {
{
return 1;
}
}
if (t_seq2(t_nm, "ctron_entry")) {
{
return 1;
}
}
if (t_seq2(t_nm, "ctron_embedded")) {
{
return 1;
}
}
if (t_seq2(t_nm, "gui_sk_load")) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "ctron_exe_path"), t_or2(t_seq2(t_nm, "env_get"), t_seq2(t_nm, "ctron_cli_flag")))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "Some"), t_or2(t_seq2(t_nm, "None"), t_or2(t_seq2(t_nm, "Ok"), t_seq2(t_nm, "Err"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "fmt"), t_or2(t_seq2(t_nm, "char_len"), t_or2(t_seq2(t_nm, "Box"), t_or2(t_seq2(t_nm, "List"), t_seq2(t_nm, "StringBuilder")))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "String"), t_or2(t_seq2(t_nm, "Channel"), t_or2(t_seq2(t_nm, "Mutex"), t_seq2(t_nm, "Atomic"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "Global"), t_or2(t_seq2(t_nm, "Task"), t_or2(t_seq2(t_nm, "Scope"), t_seq2(t_nm, "Simd"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "str_from_c"), t_or2(t_seq2(t_nm, "clo_handle"), t_or2(t_seq2(t_nm, "clo_cb2"), t_seq2(t_nm, "clo_cb3"))))) {
{
return 1;
}
}
if (t_or2(t_seq2(t_nm, "dlopen"), t_or2(t_seq2(t_nm, "dlclose"), t_seq2(t_nm, "dlsym")))) {
{
return 1;
}
}
if (t_seq2(t_nm, "errno")) {
{
return 1;
}
}
return t_or2(t_seq2(t_nm, "Option"), t_or2(t_seq2(t_nm, "Result"), t_seq2(t_nm, "AnyError")));
}
int t_value_ok(const char* t_nm) 
{
if (t_prelude_ok(t_nm)) {
{
return 1;
}
}
return t_or2(t_seq2(t_nm, "parallel"), t_or2(t_seq2(t_nm, "bit"), t_seq2(t_nm, "arena")));
}
int32_t t_prelude_arity(const char* t_nm) 
{
if (t_or2(t_seq2(t_nm, "Some"), t_or2(t_seq2(t_nm, "Ok"), t_seq2(t_nm, "Err")))) {
{
return 1;
}
}
if (t_seq2(t_nm, "None")) {
{
return 0;
}
}
return (-1);
}
const char* t_root_name(ctron_list* t_e) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
return ((const char*)((ctron_list*)(t_e))->items[1]);
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
return t_root_name((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
}
}
return "";
}
void t_bind_pat(ctron_list* t_pp, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_tn) 
{
const char* t_t = ((const char*)((ctron_list*)(t_pp))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(t_pp))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_tn));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("PatTup")) == 0)) {
{
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_pp))->n)) {
t_bind_pat((ctron_list*)(((const char*)((ctron_list*)(t_pp))->items[t_i])), t_loc, t_envT, t_unk());
t_i += 1;
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("PatAgg")) == 0)) {
{
if ((((ctron_list*)(t_pp))->n > 2)) {
{
const char* t_sub = ((const char*)((ctron_list*)(t_pp))->items[2]);
int32_t t_j = 1;
{
const char* t_c = 0;
while ((t_j < ctron_len((const void*)(t_sub)))) {
t_c = ((const char*)((ctron_list*)(t_sub))->items[t_j]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "PatFld")) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_c))->items[2]))) > 1)) {
{
t_bind_pat((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_loc, t_envT, t_unk());
}
}
else {
{
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(t_c))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
}
}
}
}
else {
{
t_bind_pat((ctron_list*)(t_c), t_loc, t_envT, t_unk());
}
}
t_j += 1;
}
}
}
}
}
}
}
}
}
const char* t_ty_kind(ctron_list* t_ty) 
{
const char* t_t = ((const char*)((ctron_list*)(t_ty))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Named")) == 0)) {
{
return ctron_str_concat((const char*)("n:"), (const char*)(((const char*)((ctron_list*)(t_ty))->items[1])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Optional")) == 0)) {
{
return ctron_str_concat((const char*)("opt:"), (const char*)(t_ty_kind((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])))));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ref")) == 0)) {
{
return ctron_str_concat((const char*)("ref:"), (const char*)(t_ty_kind((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])))));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Slice")) == 0)) {
{
return "slice";
}
}
if ((strcmp((const char*)(t_t), (const char*)("ArrayT")) == 0)) {
{
return "arr";
}
}
if ((strcmp((const char*)(t_t), (const char*)("TupleT")) == 0)) {
{
return "tup";
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("FnT")) == 0), (strcmp((const char*)(t_t), (const char*)("FnType")) == 0))) {
{
return "fn";
}
}
return "";
}
int t_k_int5(const char* t_k) 
{
return t_or2(t_seq2(t_k, "n:I8"), t_or2(t_seq2(t_k, "n:I16"), t_or2(t_seq2(t_k, "n:I32"), t_or2(t_seq2(t_k, "n:I64"), t_seq2(t_k, "n:ISize")))));
}
int t_k_uint5(const char* t_k) 
{
return t_or2(t_seq2(t_k, "n:U8"), t_or2(t_seq2(t_k, "n:U16"), t_or2(t_seq2(t_k, "n:U32"), t_or2(t_seq2(t_k, "n:U64"), t_seq2(t_k, "n:USize")))));
}
int t_k_int(const char* t_k) 
{
return t_or2(t_k_int5(t_k), t_k_uint5(t_k));
}
int t_k_num(const char* t_k) 
{
if (t_k_int(t_k)) {
{
return 1;
}
}
return t_or2(t_seq2(t_k, "n:F32"), t_seq2(t_k, "n:F64"));
}
int t_k_strish(const char* t_k) 
{
return t_or2(t_seq2(t_k, "n:Str"), t_seq2(t_k, "n:String"));
}
int t_all_int_lit(ctron_list* t_e) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Int")) == 0)) {
{
return 1;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
return t_all_int_lit((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
if (t_seq2(t_t, "Binary")) {
{
const char* t_op = ((const char*)((ctron_list*)(t_e))->items[1]);
if (t_or2(t_seq2(t_op, "Add"), t_or2(t_seq2(t_op, "Sub"), t_or2(t_seq2(t_op, "Mul"), t_or2(t_seq2(t_op, "Div"), t_or2(t_seq2(t_op, "Mod"), t_or2(t_seq2(t_op, "WrapAdd"), t_seq2(t_op, "WrapSub")))))))) {
{
return (t_all_int_lit((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2]))) && t_all_int_lit((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3]))));
}
}
}
}
return 0;
}
void t_lit_fit_gate(const char* t_expk, ctron_list* t_e, const char* t_ln, ctron_list* t_diags) 
{
if ((strlen((const char*)(t_expk)) < 3)) {
{
return;
}
}
if ((!t_seq2(ctron_byte_slice(t_expk, 0, 2), "n:"))) {
{
return;
}
}
const char* t_tn = ctron_byte_slice(t_expk, 2, strlen((const char*)(t_expk)));
int t_neg = 0;
ctron_list* t_lit = (ctron_list*)(t_e);
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[0]), "Unary")) {
{
if (t_or2((!t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Neg")), (((ctron_list*)(t_e))->n < 3))) {
{
return;
}
}
t_neg = 1;
t_lit = (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2]));
}
}
if ((!t_seq2(((const char*)((ctron_list*)(t_lit))->items[0]), "Int"))) {
{
return;
}
}
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_lit))->items[2]))) > 0)) {
{
return;
}
}
const char* t_dec = t_lit_to_dec(((const char*)((ctron_list*)(t_lit))->items[1]));
const char* t_bound = "";
if (t_k_int5(t_expk)) {
{
if (t_seq2(t_tn, "I8")) {
{
t_bound = "127";
if (t_neg) {
{
t_bound = "128";
}
}
}
}
if (t_seq2(t_tn, "I16")) {
{
t_bound = "32767";
if (t_neg) {
{
t_bound = "32768";
}
}
}
}
if (t_seq2(t_tn, "I32")) {
{
t_bound = "2147483647";
if (t_neg) {
{
t_bound = "2147483648";
}
}
}
}
if (t_seq2(t_tn, "I64")) {
{
t_bound = "9223372036854775807";
if (t_neg) {
{
t_bound = "9223372036854775808";
}
}
}
}
if (t_seq2(t_tn, "ISize")) {
{
t_bound = "9223372036854775807";
if (t_neg) {
{
t_bound = "9223372036854775808";
}
}
}
}
}
}
if (t_k_uint5(t_expk)) {
{
if (t_seq2(t_tn, "U8")) {
{
t_bound = "255";
}
}
if (t_seq2(t_tn, "U16")) {
{
t_bound = "65535";
}
}
if (t_seq2(t_tn, "U32")) {
{
t_bound = "4294967295";
}
}
if (t_or2(t_seq2(t_tn, "U64"), t_seq2(t_tn, "USize"))) {
{
t_bound = "18446744073709551615";
}
}
if (t_neg) {
{
t_bound = "";
}
}
}
}
if ((strlen((const char*)(t_bound)) == 0)) {
{
return;
}
}
if ((t_c6abscmp(t_dec, t_bound) <= 0)) {
{
return;
}
}
t_diag2(t_diags, "E2040", t_ln, t_expk, ((const char*)((ctron_list*)(t_lit))->items[1]));
}
int t_compat(const char* t_exp, const char* t_act, ctron_list* t_e, ctron_list* t_file, ctron_list* t_tc) 
{
if ((strlen((const char*)(t_exp)) == 0)) {
{
return 1;
}
}
if ((strlen((const char*)(t_act)) == 0)) {
{
return 1;
}
}
if (t_seq2(t_exp, t_act)) {
{
return 1;
}
}
if (t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[13])), t_exp)) {
{
return 1;
}
}
if (((strlen((const char*)(t_exp)) > 4) && t_seq2(ctron_byte_slice(t_exp, 0, 4), "ref:"))) {
{
const char* t_tn = ctron_byte_slice(t_exp, 4, strlen((const char*)(t_exp)));
if (t_seq2(ctron_byte_slice(t_tn, 0, 2), "n:")) {
{
if (t_is_trait(t_file, ctron_byte_slice(t_tn, 2, strlen((const char*)(t_tn))))) {
{
return 1;
}
}
}
}
}
}
if (t_seq2(ctron_byte_slice(t_exp, 0, 2), "n:")) {
{
if (t_is_trait(t_file, ctron_byte_slice(t_exp, 2, strlen((const char*)(t_exp))))) {
{
return 1;
}
}
}
}
if (t_all_int_lit(t_e)) {
{
return t_k_num(t_exp);
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Int")) == 0)) {
{
return t_k_num(t_exp);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Float")) == 0)) {
{
return t_or2(t_seq2(t_exp, "n:F32"), t_seq2(t_exp, "n:F64"));
}
}
if (((strlen((const char*)(t_exp)) >= 4) && t_seq2(ctron_byte_slice(t_exp, 0, 4), "opt:"))) {
{
return t_seq2(t_exp, ctron_str_concat((const char*)("opt:"), (const char*)(t_act)));
}
}
if (t_seq2(t_exp, "n:Str")) {
{
return t_seq2(t_act, "n:String");
}
}
if (t_seq2(t_exp, "n:I64")) {
{
return t_seq2(t_act, "n:I32");
}
}
if (t_seq2(t_exp, "slice")) {
{
return t_seq2(t_act, "arr");
}
}
if (t_seq2(t_exp, "ref:slice")) {
{
return t_or2(t_seq2(t_act, "slice"), t_seq2(t_act, "arr"));
}
}
return 0;
}
const char* t_nline(ctron_list* t_n) 
{
if ((((ctron_list*)(t_n))->n == 0)) {
{
return "0";
}
}
const char* t_last = ((const char*)((ctron_list*)(t_n))->items[(((ctron_list*)(t_n))->n - 1)]);
int32_t t_i = 0;
{
int32_t t_c = 0;
int t_dig = 0;
while ((t_i < ctron_len((const void*)(t_last)))) {
t_c = ctron_byte_at(t_last, t_i);
t_dig = 1;
if ((t_c < 48)) {
{
t_dig = 0;
}
}
if ((t_c > 57)) {
{
t_dig = 0;
}
}
if ((!t_dig)) {
{
return "0";
}
}
t_i += 1;
}
}
if ((ctron_len((const void*)(t_last)) == 0)) {
{
return "0";
}
}
return t_last;
}
ctron_list* t_unk() 
{
ctron_list* t_u = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_u), "Unknown");
return t_u;
}
ctron_list* t_kind_node(const char* t_k) 
{
if (((strlen((const char*)(t_k)) >= 2) && t_seq2(ctron_byte_slice(t_k, 0, 2), "n:"))) {
{
ctron_list* t_nt = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_nt), "Named");
ctron_list_push((ctron_list*)(t_nt), ctron_byte_slice(t_k, 2, strlen((const char*)(t_k))));
ctron_list_push((ctron_list*)(t_nt), (char*)(ctron_list_new()));
return t_nt;
}
}
return t_unk();
}
int32_t t_idx_of(ctron_list* t_ls, const char* t_nm) 
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_ls))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_ls))->items[t_i]), t_nm)) {
{
return t_i;
}
}
t_i += 1;
}
return (-1);
}
ctron_list* t_fn_ret_of(ctron_list* t_tc, const char* t_nm) 
{
int32_t t_g = 0;
while ((t_g < ctron_len((const void*)(((const char*)((ctron_list*)(t_tc))->items[0]))))) {
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])))->items[t_g]), t_nm)) {
{
return ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[2])))->items[t_g]);
}
}
t_g += 1;
}
return ctron_list_new();
}
const char* t_ex_ty(ctron_list* t_e, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Int")) == 0)) {
{
return "n:I32";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Float")) == 0)) {
{
return "n:F64";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Str")) == 0)) {
{
return "n:Str";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Bool")) == 0)) {
{
return "n:Bool";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Void")) == 0)) {
{
return "void";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
return "range";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
const char* t_nm = ((const char*)((ctron_list*)(t_e))->items[1]);
ctron_list* t_b = (ctron_list*)(t_env_ty(t_loc, t_envT, t_nm));
if ((((ctron_list*)(t_b))->n > 0)) {
{
return t_ty_kind(t_b);
}
}
ctron_list* t_g = (ctron_list*)(t_env_ty((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[4])), (ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[5])), t_nm));
if ((((ctron_list*)(t_g))->n > 0)) {
{
return t_ty_kind(t_g);
}
}
if (t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])), t_nm)) {
{
return "fn";
}
}
return "";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
return "fn";
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
return ctron_str_concat((const char*)("n:"), (const char*)(((const char*)((ctron_list*)(t_e))->items[1])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_rt = t_root_name((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strlen((const char*)(t_rt)) > 0)) {
{
ctron_list* t_r = (ctron_list*)(t_fn_ret_of(t_tc, t_rt));
if ((((ctron_list*)(t_r))->n > 0)) {
{
return t_ty_kind(t_r);
}
}
}
}
return "";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Not")) {
{
return "n:Bool";
}
}
return t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
const char* t_a = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
const char* t_b2 = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT);
if (t_seq2(t_a, t_b2)) {
{
return t_a;
}
}
return "";
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_op = ((const char*)((ctron_list*)(t_e))->items[1]);
if (t_is_cmp_op(t_op)) {
{
return "n:Bool";
}
}
if (t_seq2(t_op, "AndAnd")) {
{
return "n:Bool";
}
}
if (t_seq2(t_op, "OrOr")) {
{
return "n:Bool";
}
}
const char* t_lt = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
const char* t_rt2 = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT);
if (((t_seq2(t_op, "Add") && t_k_strish(t_lt)) && t_k_strish(t_rt2))) {
{
return "n:Str";
}
}
if ((t_k_num(t_lt) && t_k_num(t_rt2))) {
{
if (t_seq2(t_lt, t_rt2)) {
{
return t_lt;
}
}
return "";
}
}
if (t_seq2(t_lt, t_rt2)) {
{
return t_lt;
}
}
return "";
}
}
return "";
}
const char* t_gui_view_props(ctron_list* t_file, const char* t_nm, ctron_list* t_found) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("GuiBlock")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[1])), (const char*)("view")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[2])), (const char*)(t_nm)) == 0)) {
{
((ctron_list*)(t_found))->items[0] = "1";
if ((ctron_len((const void*)(t_d)) > 5)) {
{
return ((const char*)((ctron_list*)(t_d))->items[5]);
}
}
return "";
}
}
}
}
}
}
t_i += 1;
}
}
return "";
}
void t_tce(ctron_list* t_file, ctron_list* t_e, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_retK, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
const char* t_rt = t_root_name((ctron_list*)(t_cal));
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_got = (ctron_len((const void*)(t_ag)) - 1);
ctron_list* t_vfound = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_vfound), "0");
const char* t_vprops = t_gui_view_props(t_file, t_rt, t_vfound);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_vfound))->items[0])), (const char*)("1")) == 0)) {
{
ctron_list* t_dn = (ctron_list*)(ctron_list_new());
ctron_list* t_dt = (ctron_list*)(ctron_list_new());
t_gui_props_typed(t_vprops, t_dn, t_dt);
ctron_list* t_have = (ctron_list*)(ctron_list_new());
int t_badnp = 0;
int32_t t_ai = 1;
{
const char* t_a = 0;
while ((t_ai < ctron_len((const void*)(t_ag)))) {
t_a = ((const char*)((ctron_list*)(t_ag))->items[t_ai]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_a))->items[0])), (const char*)("NParg")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
if (t_gui_ck_has_style(t_dn, ((const char*)((ctron_list*)(t_a))->items[1]))) {
{
if (t_gui_ck_has_style(t_have, ((const char*)((ctron_list*)(t_a))->items[1]))) {
{
t_diag2(t_diags, "E8110.dup", t_nline((ctron_list*)(t_a)), ctron_str_concat((const char*)("组件 prop 重复: "), (const char*)(((const char*)((ctron_list*)(t_a))->items[1]))), "");
t_badnp = 1;
}
}
else {
{
ctron_list_push((ctron_list*)(t_have), ((const char*)((ctron_list*)(t_a))->items[1]));
}
}
}
}
else {
{
t_diag2(t_diags, "E8110.np", t_nline((ctron_list*)(t_a)), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("组件未知 prop: "), (const char*)(((const char*)((ctron_list*)(t_a))->items[1])))), (const char*)(" 于 "))), (const char*)(t_rt)), "");
t_badnp = 1;
}
}
}
}
else {
{
t_diag2(t_diags, "E8110.np", t_nline((ctron_list*)(t_a)), ctron_str_concat((const char*)("组件实参须命名形态(prop: expr) 于 "), (const char*)(t_rt)), "");
t_badnp = 1;
}
}
t_ai += 1;
}
}
if ((!t_badnp)) {
{
int32_t t_di = 0;
while ((t_di < ((ctron_list*)(t_dn))->n)) {
if ((!t_gui_ck_has_style(t_have, ((const char*)((ctron_list*)(t_dn))->items[t_di])))) {
{
t_diag2(t_diags, "E8100.props", t_nline(t_e), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("组件必填 prop 缺失: "), (const char*)(((const char*)((ctron_list*)(t_dn))->items[t_di])))), (const char*)(" 于 "))), (const char*)(t_rt)), "");
}
}
t_di += 1;
}
}
}
return;
}
}
if ((((strlen((const char*)(t_rt)) > 0) && (!t_in_list(t_loc, t_rt))) && (!t_in_list(t_diags, ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("E2020@"), (const char*)(t_nline(t_e)))), (const char*)(": 未解析的名称(unresolved):"))), (const char*)(t_rt)))))) {
{
if ((((!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])), t_rt)) && (!t_prelude_ok(t_rt))) && (!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[6])), t_rt)))) {
{
t_diag1(t_diags, "E2020.name", t_nline(t_e), t_rt);
}
}
else {
{
int32_t t_g = 0;
while ((t_g < ctron_len((const void*)(((const char*)((ctron_list*)(t_tc))->items[0]))))) {
if ((t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])))->items[t_g]), t_rt) && (ctron_len((const void*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[1])))->items[t_g]))) > 0))) {
{
if ((t_got != t_dvi(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[1])))->items[t_g])))) {
{
t_diag3(t_diags, "E2010.arity", t_nline(t_e), t_rt, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[1])))->items[t_g]), ctron_i32_to_string((int32_t)(t_got)));
}
}
const char* t_ps = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[3])))->items[t_g]);
int32_t t_i = 1;
{
const char* t_prm = 0;
while (((t_i < ctron_len((const void*)(t_ps))) && (t_i <= t_got))) {
t_prm = ((const char*)((ctron_list*)(t_ps))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_prm))->items[0])), (const char*)("Param")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])))->items[0])), (const char*)("NParg")) == 0)) {
{
t_diag2(t_diags, "E8110.np", t_nline((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i]))), ctron_str_concat((const char*)("命名实参仅用于组件实例化,于 "), (const char*)(t_rt)), "");
}
}
else {
{
const char* t_pk = t_ty_kind((ctron_list*)(((const char*)((ctron_list*)(t_prm))->items[3])));
const char* t_ak = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_tc, t_loc, t_envT);
if ((!t_compat(t_pk, t_ak, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_file, t_tc))) {
{
t_diag5(t_diags, "E2010.arg", t_nline((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i]))), t_rt, ctron_i32_to_string((int32_t)(t_i)), t_root_name((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i]))), t_pk, t_ak);
}
}
t_lit_fit_gate(t_pk, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), "0", t_diags);
}
}
}
}
t_i += 1;
}
}
}
}
t_g += 1;
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0)) {
{
t_check_tpar_bounds(t_file, t_e, (ctron_list*)(t_cal), t_rt, t_diags);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
ctron_list* t_gd2 = (ctron_list*)(t_find_gdecl(t_file, t_rt));
if (((((ctron_list*)(t_gd2))->n > 0) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_gd2))->items[2]))) >= 2))) {
{
const char* t_tps2 = ((const char*)((ctron_list*)(t_gd2))->items[2]);
const char* t_ps2 = ((const char*)((ctron_list*)(t_gd2))->items[3]);
ctron_list* t_binds = (ctron_list*)(ctron_list_new());
ctron_list* t_bvals = (ctron_list*)(ctron_list_new());
int t_amb2 = 0;
int t_sawc = 0;
int32_t t_pi2 = 1;
{
const char* t_prm2 = 0;
while (((t_pi2 < ctron_len((const void*)(t_ps2))) && (t_pi2 <= t_got))) {
t_prm2 = ((const char*)((ctron_list*)(t_ps2))->items[t_pi2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_prm2))->items[0])), (const char*)("Param")) == 0)) {
{
const char* t_ft = ((const char*)((ctron_list*)(t_prm2))->items[3]);
const char* t_ak2 = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_pi2])), t_tc, t_loc, t_envT);
if ((strlen((const char*)(t_ak2)) > 0)) {
{
t_sawc = 1;
}
}
const char* t_tn = "";
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_ft))->items[0])), (const char*)("Ident")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_ft))->items[0])), (const char*)("Named")) == 0))) {
{
int32_t t_k2 = 1;
while ((t_k2 < ctron_len((const void*)(t_tps2)))) {
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps2))->items[t_k2])))->items[1]), ((const char*)((ctron_list*)(t_ft))->items[1]))) {
{
t_tn = ((const char*)((ctron_list*)(t_ft))->items[1]);
}
}
t_k2 += 1;
}
}
}
if (((strlen((const char*)(t_tn)) > 0) && (strlen((const char*)(t_ak2)) > 0))) {
{
if (t_in_list(t_binds, t_tn)) {
{
int32_t t_hit2 = 0;
int32_t t_bi2 = 0;
while ((t_bi2 < ((ctron_list*)(t_binds))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_binds))->items[t_bi2]), t_tn)) {
{
t_hit2 = t_bi2;
}
}
t_bi2 += 1;
}
if ((!t_seq2(((const char*)((ctron_list*)(t_bvals))->items[t_hit2]), t_ak2))) {
{
t_amb2 = 1;
}
}
}
}
else {
{
ctron_list_push((ctron_list*)(t_binds), t_tn);
ctron_list_push((ctron_list*)(t_bvals), t_ak2);
}
}
}
}
}
}
t_pi2 += 1;
}
}
if (t_amb2) {
{
t_diag1(t_diags, "E2061", t_nline(t_e), t_rt);
}
}
else {
{
const char* t_miss = "";
int32_t t_k3 = 1;
while ((t_k3 < ctron_len((const void*)(t_tps2)))) {
if ((!t_in_list(t_binds, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps2))->items[t_k3])))->items[1])))) {
{
if ((strlen((const char*)(t_miss)) > 0)) {
{
t_miss = ctron_str_concat((const char*)(t_miss), (const char*)(", "));
}
}
t_miss = ctron_str_concat((const char*)(t_miss), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps2))->items[t_k3])))->items[1])));
}
}
t_k3 += 1;
}
if (((strlen((const char*)(t_miss)) > 0) && t_sawc)) {
{
t_diag2(t_diags, "E2060", t_nline(t_e), t_miss, t_rt);
}
}
}
}
}
}
}
}
if ((!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])), t_rt))) {
{
int32_t t_pa = t_prelude_arity(t_rt);
if (((t_pa >= 0) && (t_got != t_pa))) {
{
t_diag3(t_diags, "E2010.arity", t_nline(t_e), t_rt, ctron_i32_to_string((int32_t)(t_pa)), ctron_i32_to_string((int32_t)(t_got)));
}
}
int32_t t_vi = t_idx_of((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[6])), t_rt);
if ((t_vi >= 0)) {
{
if ((t_got != t_dvi(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[7])))->items[t_vi])))) {
{
t_diag3(t_diags, "E2010.arity", t_nline(t_e), t_rt, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[7])))->items[t_vi]), ctron_i32_to_string((int32_t)(t_got)));
}
}
}
}
}
}
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
ctron_list_push((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[10])), ((const char*)((ctron_list*)(t_cal))->items[1]));
}
}
else {
{
t_tce(t_file, (ctron_list*)(t_cal), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
int32_t t_i2 = 1;
while ((t_i2 < ctron_len((const void*)(t_ag)))) {
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
const char* t_nm = ((const char*)((ctron_list*)(t_e))->items[1]);
ctron_list_push((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[10])), t_nm);
if (((((((!t_in_list(t_loc, t_nm)) && (!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[0])), t_nm))) && (!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[4])), t_nm))) && (!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[6])), t_nm))) && (!t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[8])), t_nm))) && (!t_value_ok(t_nm)))) {
{
t_donce1(t_diags, "E2020.name", t_nline(t_e), t_nm);
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_op = ((const char*)((ctron_list*)(t_e))->items[1]);
const char* t_lt = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
const char* t_rt2 = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT);
int t_bad = 0;
const char* t_m = "";
if (t_or2(t_seq2(t_op, "Sub"), t_or2(t_seq2(t_op, "Mul"), t_or2(t_seq2(t_op, "Div"), t_or2(t_seq2(t_op, "Mod"), t_seq2(t_op, "WrapSub")))))) {
{
if (t_or2(t_k_strish(t_lt), t_k_strish(t_rt2))) {
{
t_bad = 1;
t_m = t_diag_render1("E2010.arith.str", t_nline(t_e), t_op);
}
}
}
}
if (t_seq2(t_op, "Add")) {
{
int t_lunk = t_seq2(t_lt, "");
int t_runk = t_seq2(t_rt2, "");
if (((!t_lunk) && (!t_runk))) {
{
if ((t_k_strish(t_lt) != t_k_strish(t_rt2))) {
{
t_bad = 1;
t_m = t_diag_render0("E2010.concat", t_nline(t_e));
}
}
}
}
}
}
if (t_or2(t_seq2(t_op, "Add"), t_or2(t_seq2(t_op, "Sub"), t_or2(t_seq2(t_op, "Mul"), t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "Mod")))))) {
{
if (t_or2(t_seq2(t_lt, "n:Bool"), t_seq2(t_rt2, "n:Bool"))) {
{
t_bad = 1;
t_m = t_diag_render1("E2010.arith.bool", t_nline(t_e), t_op);
}
}
}
}
if (t_seq2(t_op, "AndAnd")) {
{
if (t_or2(t_k_num(t_lt), t_or2(t_k_strish(t_lt), t_or2(t_k_num(t_rt2), t_k_strish(t_rt2))))) {
{
t_bad = 1;
t_m = t_diag_render0("E2010.op.and", t_nline(t_e));
}
}
}
}
if (t_seq2(t_op, "OrOr")) {
{
if (t_or2(t_k_num(t_lt), t_or2(t_k_strish(t_lt), t_or2(t_k_num(t_rt2), t_k_strish(t_rt2))))) {
{
t_bad = 1;
t_m = t_diag_render0("E2010.op.or", t_nline(t_e));
}
}
}
}
if (t_bad) {
{
ctron_list_push((ctron_list*)(t_diags), t_m);
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Not")) {
{
const char* t_nt = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
if (t_or2(t_k_num(t_nt), t_k_strish(t_nt))) {
{
const char* t_nln = "";
if ((!t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[0]), "Str"))) {
{
t_nln = t_nline((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
ctron_list_push((ctron_list*)(t_diags), t_diag_render0("E2010.op.not", t_nln));
}
}
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Neg")) {
{
const char* t_ng = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT);
if (t_or2(t_k_strish(t_ng), t_seq2(t_ng, "n:Bool"))) {
{
const char* t_gln = "";
if ((!t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[0]), "Str"))) {
{
t_gln = t_nline((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
ctron_list_push((ctron_list*)(t_diags), t_diag_render0("E2010.op.neg", t_gln));
}
}
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
if ((((ctron_list*)(t_retK))->n > 0)) {
{
const char* t_rk = t_ty_kind(t_retK);
if ((strlen((const char*)(t_rk)) > 0)) {
{
int t_okr = 0;
if ((strlen((const char*)(t_rk)) >= 4)) {
{
t_okr = t_seq2(ctron_byte_slice(t_rk, 0, 4), "opt:");
}
}
t_okr = t_or2(t_okr, t_or2(t_seq2(t_rk, "n:Result"), t_or2(t_seq2(t_rk, "n:AnyError"), t_seq2(t_rk, "n:Option"))));
if ((!t_okr)) {
{
t_diag1(t_diags, "E2010.prop", t_nline(t_e), t_rk);
}
}
}
}
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i3 = 1;
while ((t_i3 < ((ctron_list*)(t_e))->n)) {
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i3])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
const char* t_ck = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT);
if (((strlen((const char*)(t_ck)) > 0) && (!t_seq2(t_ck, "n:Bool")))) {
{
t_diag0(t_diags, "E2010.cond.if", t_nline(t_e));
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i4 = 1;
{
const char* t_a = 0;
while ((t_i4 < ctron_len((const void*)(t_arms)))) {
t_a = ((const char*)((ctron_list*)(t_arms))->items[t_i4]);
t_bind_pat((ctron_list*)(((const char*)((ctron_list*)(t_a))->items[1])), t_loc, t_envT, t_unk());
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_a))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_i4 += 1;
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
const char* t_cp = ((const char*)((ctron_list*)(t_e))->items[1]);
if (((((ctron_list*)(t_retK))->n > 0) && t_seq2(((const char*)((ctron_list*)(t_retK))->items[0]), "FnType"))) {
{
const char* t_fp = ((const char*)((ctron_list*)(t_retK))->items[1]);
int32_t t_i8 = 1;
while ((t_i8 < ctron_len((const void*)(t_cp)))) {
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cp))->items[t_i8])))->items[2]));
if ((t_i8 < ctron_len((const void*)(t_fp)))) {
{
ctron_list_push((ctron_list*)(t_envT), ((const char*)((ctron_list*)(t_fp))->items[t_i8]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
}
}
t_i8 += 1;
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT, (ctron_list*)(((const char*)((ctron_list*)(t_retK))->items[2])), t_diags);
return;
}
}
int32_t t_i5 = 1;
while ((t_i5 < ctron_len((const void*)(t_cp)))) {
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cp))->items[t_i5])))->items[2]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
t_i5 += 1;
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(t_e))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(t_e))->items[1]));
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i6 = 1;
{
const char* t_c = 0;
while ((t_i6 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i6]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
t_i6 += 1;
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Str")) == 0)) {
{
const char* t_pts = ((const char*)((ctron_list*)(t_e))->items[1]);
int32_t t_i7 = 1;
{
const char* t_pt = 0;
while ((t_i7 < ctron_len((const void*)(t_pts)))) {
t_pt = ((const char*)((ctron_list*)(t_pts))->items[t_i7]);
if ((t_seq2(((const char*)((ctron_list*)(t_pt))->items[0]), "Interp") && (ctron_len((const void*)(((const char*)((ctron_list*)(t_pt))->items[1]))) > 0))) {
{
ctron_list_push((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[11])), ((const char*)((ctron_list*)(t_pt))->items[1]));
}
}
t_i7 += 1;
}
}
return;
}
}
}
void t_tcb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_tc, ctron_list* t_loc, ctron_list* t_envT, ctron_list* t_retK, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_s = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_s = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_s))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_ann = ((const char*)((ctron_list*)(t_s))->items[3]);
int t_hasann = ((ctron_len((const void*)(t_ann)) > 0) && (!t_seq2(((const char*)((ctron_list*)(t_ann))->items[0]), "None")));
if (t_hasann) {
{
const char* t_act = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_tc, t_loc, t_envT);
if ((!t_compat(t_ty_kind((ctron_list*)(t_ann)), t_act, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_file, t_tc))) {
{
t_diag2(t_diags, "E2010.let", t_nline((ctron_list*)(t_s)), t_ty_kind((ctron_list*)(t_ann)), t_act);
}
}
t_lit_fit_gate(t_ty_kind((ctron_list*)(t_ann)), (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_nline((ctron_list*)(t_s)), t_diags);
const char* t_abk = ((const char*)((ctron_list*)(t_ann))->items[0]);
if ((strcmp((const char*)(t_abk), (const char*)("Named")) == 0)) {
{
ctron_list* t_asd = (ctron_list*)(t_find_sdecl(t_file, ((const char*)((ctron_list*)(t_ann))->items[1])));
if ((((ctron_list*)(t_asd))->n > 0)) {
{
const char* t_atps = ((const char*)((ctron_list*)(t_asd))->items[3]);
if (((ctron_len((const void*)(t_atps)) > 1) && (strcmp((const char*)(((const char*)((ctron_list*)(t_atps))->items[0])), (const char*)("TPs")) == 0))) {
{
const char* t_atas = ((const char*)((ctron_list*)(t_ann))->items[2]);
if ((ctron_len((const void*)(t_atas)) == ctron_len((const void*)(t_atps)))) {
{
int32_t t_ak = 1;
{
const char* t_abds = 0;
while ((t_ak < ctron_len((const void*)(t_atps)))) {
t_abds = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_atps))->items[t_ak])))->items[3]);
int32_t t_abi = 1;
while ((t_abi < ctron_len((const void*)(t_abds)))) {
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_abds))->items[t_abi]), "Show"), t_seq2(((const char*)((ctron_list*)(t_abds))->items[t_abi]), "Eq"))) {
{
if ((!t_bound_sat(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_atas))->items[t_ak])), ((const char*)((ctron_list*)(t_abds))->items[t_abi]), 0))) {
{
const char* t_anm2 = "?";
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_atas))->items[t_ak])))->items[0])), (const char*)("Named")) == 0)) {
{
t_anm2 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_atas))->items[t_ak])))->items[1]);
}
}
t_diag4(t_diags, "E2050", t_nline((ctron_list*)(t_s)), ((const char*)((ctron_list*)(t_ann))->items[1]), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_atps))->items[t_ak])))->items[1]), ((const char*)((ctron_list*)(t_abds))->items[t_abi]), t_anm2);
}
}
}
}
t_abi += 1;
}
t_ak += 1;
}
}
}
}
}
}
}
}
}
}
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_tc, t_loc, t_envT, t_retK, t_diags);
ctron_list* t_tn = (ctron_list*)(ctron_list_new());
if (t_hasann) {
{
t_tn = (ctron_list*)(t_ann);
}
}
else {
{
t_tn = (ctron_list*)(t_kind_node(t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_tc, t_loc, t_envT)));
}
}
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])))->items[0]), "PatId")) {
{
const char* t_vn = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])))->items[1]);
ctron_list_push((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[9])), t_vn);
if (t_in_list((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[12])), t_vn)) {
{
t_diag1(t_diags, "W8040", t_nline((ctron_list*)(t_s)), t_vn);
}
}
const char* t_dty = "";
if (t_hasann) {
{
t_dty = t_ty_kind((ctron_list*)(t_ann));
}
}
else {
{
t_dty = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[4])), t_tc, t_loc, t_envT);
}
}
if (((strlen((const char*)(t_dty)) > 3) && (strcmp((const char*)(ctron_byte_slice(t_dty, 0, 2)), (const char*)("n:")) == 0))) {
{
if (t_has_drop_impl(t_file, ctron_byte_slice(t_dty, 2, strlen((const char*)(t_dty))))) {
{
ctron_list_push((ctron_list*)(((const char*)((ctron_list*)(t_tc))->items[10])), t_vn);
}
}
}
}
}
}
t_bind_pat((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_loc, t_envT, t_tn);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
const char* t_ck = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT);
if (((strlen((const char*)(t_ck)) > 0) && (!t_seq2(t_ck, "n:Bool")))) {
{
t_diag0(t_diags, "E2010.cond.while", t_nline((ctron_list*)(t_s)));
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_bind_pat((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_loc, t_envT, t_unk());
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[2])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_s))->items[1]))) > 1)) {
{
if (((((ctron_list*)(t_retK))->n > 0) && (!t_seq2(((const char*)((ctron_list*)(t_retK))->items[0]), "None")))) {
{
const char* t_act = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT);
if ((!t_compat(t_ty_kind(t_retK), t_act, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_file, t_tc))) {
{
t_diag2(t_diags, "E2010.ret", t_nline((ctron_list*)(t_s)), t_ty_kind(t_retK), t_act);
}
}
t_lit_fit_gate(t_ty_kind(t_retK), (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_nline((ctron_list*)(t_s)), t_diags);
}
}
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
const char* t_tk = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT);
const char* t_vk = t_ex_ty((ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_tc, t_loc, t_envT);
if ((!t_compat(t_tk, t_vk, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_file, t_tc))) {
{
t_diag2(t_diags, "E2010.assign", t_nline((ctron_list*)(t_s)), t_tk, t_vk);
}
}
t_lit_fit_gate(t_tk, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_nline((ctron_list*)(t_s)), t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[3])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_tce(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_s))->items[1])), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
else {
if ((t_seq2(t_t, "None") && (ctron_len((const void*)(t_s)) == 1))) {
{
}
}
else {
{
t_tce(t_file, (ctron_list*)(t_s), t_tc, t_loc, t_envT, t_retK, t_diags);
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
void t_fn_params_push(ctron_list* t_ps, ctron_list* t_loc, ctron_list* t_envT) 
{
int32_t t_i = 1;
{
const char* t_p = 0;
while ((t_i < ((ctron_list*)(t_ps))->n)) {
t_p = ((const char*)((ctron_list*)(t_ps))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Param")) == 0)) {
{
ctron_list_push((ctron_list*)(t_loc), ((const char*)((ctron_list*)(t_p))->items[2]));
ctron_list_push((ctron_list*)(t_envT), ((const char*)((ctron_list*)(t_p))->items[3]));
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Receiver")) == 0)) {
{
ctron_list_push((ctron_list*)(t_loc), "self");
ctron_list_push((ctron_list*)(t_envT), (char*)(t_unk()));
}
}
}
t_i += 1;
}
}
}
ctron_list* t_find_sdecl(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Struct")) == 0) && t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm))) {
{
return t_d;
}
}
t_i += 1;
}
}
return ctron_list_new();
}
int t_has_drop_impl(ctron_list* t_file, const char* t_tnm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0)) {
{
const char* t_trh = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])));
if (t_seq2(t_trh, "Drop")) {
{
const char* t_fh = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])));
if (t_seq2(t_fh, t_tnm)) {
{
return 1;
}
}
}
}
}
}
t_i += 1;
}
}
return 0;
}
ctron_list* t_find_gdecl(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnPub")) == 0))) {
{
if (t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm)) {
{
return t_d;
}
}
}
}
t_i += 1;
}
}
return ctron_list_new();
}
ctron_list* t_subst_tpar_ty(ctron_list* t_ty, ctron_list* t_tps, ctron_list* t_args) 
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Named")) == 0)) {
{
int32_t t_k = 1;
while ((t_k < ((ctron_list*)(t_tps))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_ty))->items[1]), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps))->items[t_k])))->items[1]))) {
{
return ((const char*)((ctron_list*)(t_args))->items[t_k]);
}
}
t_k += 1;
}
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_ty))->items[2]))) > 1)) {
{
ctron_list* t_nt = (ctron_list*)(t_mk("Named"));
ctron_list_push((ctron_list*)(t_nt), ((const char*)((ctron_list*)(t_ty))->items[1]));
ctron_list* t_ta = (ctron_list*)(t_mk("TArgs"));
int32_t t_m = 1;
while ((t_m < ctron_len((const void*)(((const char*)((ctron_list*)(t_ty))->items[2]))))) {
ctron_list_push((ctron_list*)(t_ta), (char*)(t_subst_tpar_ty((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[2])))->items[t_m])), t_tps, t_args)));
t_m += 1;
}
ctron_list_push((ctron_list*)(t_nt), (char*)(t_ta));
return t_nt;
}
}
}
}
return t_ty;
}
int t_bound_sat(ctron_list* t_file, ctron_list* t_ty, const char* t_tr, int32_t t_dp) 
{
if ((t_dp > 6)) {
{
return 0;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ty))->items[0])), (const char*)("Named")) != 0)) {
{
return 0;
}
}
if (t_seq2(t_tr, "Eq")) {
{
const char* t_nm0 = ((const char*)((ctron_list*)(t_ty))->items[1]);
if (t_or2(t_seq2(t_nm0, "I32"), t_or2(t_seq2(t_nm0, "I64"), t_or2(t_seq2(t_nm0, "Bool"), t_seq2(t_nm0, "Str"))))) {
{
return 1;
}
}
}
}
ctron_list* t_sd = (ctron_list*)(t_find_sdecl(t_file, ((const char*)((ctron_list*)(t_ty))->items[1])));
if ((((ctron_list*)(t_sd))->n == 0)) {
{
if ((t_dp == 0)) {
{
const char* t_nm9 = ((const char*)((ctron_list*)(t_ty))->items[1]);
int t_isprim = t_or2(t_seq2(t_nm9, "I32"), t_or2(t_seq2(t_nm9, "I64"), t_or2(t_seq2(t_nm9, "Bool"), t_or2(t_seq2(t_nm9, "Str"), t_or2(t_seq2(t_nm9, "F64"), t_seq2(t_nm9, "F32"))))));
if ((!t_isprim)) {
{
int32_t t_i9 = 1;
int t_isdecl = 0;
{
const char* t_d9 = 0;
while ((t_i9 < ((ctron_list*)(t_file))->n)) {
t_d9 = ((const char*)((ctron_list*)(t_file))->items[t_i9]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d9))->items[0])), (const char*)("Enum")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d9))->items[0])), (const char*)("Class")) == 0))) {
{
if (t_seq2(((const char*)((ctron_list*)(t_d9))->items[1]), t_nm9)) {
{
t_isdecl = 1;
}
}
}
}
t_i9 += 1;
}
}
if ((!t_isdecl)) {
{
return 1;
}
}
}
}
}
}
return 0;
}
}
const char* t_tps = ((const char*)((ctron_list*)(t_sd))->items[3]);
const char* t_args = ((const char*)((ctron_list*)(t_ty))->items[2]);
if (((ctron_len((const void*)(t_tps)) > 1) && (ctron_len((const void*)(t_args)) != ctron_len((const void*)(t_tps))))) {
{
return 0;
}
}
const char* t_fs = ((const char*)((ctron_list*)(t_sd))->items[4]);
int32_t t_j = 1;
{
const char* t_ft = 0;
int t_prim = 0;
while ((t_j < ctron_len((const void*)(t_fs)))) {
t_ft = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_fs))->items[t_j])))->items[3]);
if ((ctron_len((const void*)(t_tps)) > 1)) {
{
t_ft = t_subst_tpar_ty((ctron_list*)(t_ft), (ctron_list*)(t_tps), (ctron_list*)(t_args));
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ft))->items[0])), (const char*)("Named")) != 0)) {
{
return 0;
}
}
t_prim = t_or2(t_seq2(((const char*)((ctron_list*)(t_ft))->items[1]), "I32"), t_or2(t_seq2(((const char*)((ctron_list*)(t_ft))->items[1]), "I64"), t_or2(t_seq2(((const char*)((ctron_list*)(t_ft))->items[1]), "Bool"), t_seq2(((const char*)((ctron_list*)(t_ft))->items[1]), "Str"))));
if ((!t_prim)) {
{
if ((!t_bound_sat(t_file, (ctron_list*)(t_ft), t_tr, (t_dp + 1)))) {
{
return 0;
}
}
}
}
t_j += 1;
}
}
return 1;
}
void t_check_tpar_bounds(ctron_list* t_file, ctron_list* t_e, ctron_list* t_cal, const char* t_rt, ctron_list* t_diags) 
{
ctron_list* t_gd = (ctron_list*)(t_find_gdecl(t_file, t_rt));
if ((((ctron_list*)(t_gd))->n == 0)) {
{
return;
}
}
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_gd))->items[2]))) < 2)) {
{
return;
}
}
const char* t_tps = ((const char*)((ctron_list*)(t_gd))->items[2]);
const char* t_tas = ((const char*)((ctron_list*)(t_cal))->items[2]);
if ((ctron_len((const void*)(t_tas)) != ctron_len((const void*)(t_tps)))) {
{
return;
}
}
int32_t t_k = 1;
{
const char* t_bds = 0;
while ((t_k < ctron_len((const void*)(t_tps)))) {
t_bds = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps))->items[t_k])))->items[3]);
int32_t t_bi = 1;
while ((t_bi < ctron_len((const void*)(t_bds)))) {
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_bds))->items[t_bi]), "Show"), t_seq2(((const char*)((ctron_list*)(t_bds))->items[t_bi]), "Eq"))) {
{
if ((!t_bound_sat(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_tas))->items[t_k])), ((const char*)((ctron_list*)(t_bds))->items[t_bi]), 0))) {
{
const char* t_anm = "?";
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tas))->items[t_k])))->items[0])), (const char*)("Named")) == 0)) {
{
t_anm = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tas))->items[t_k])))->items[1]);
}
}
t_diag4(t_diags, "E2050", t_nline(t_e), t_rt, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps))->items[t_k])))->items[1]), ((const char*)((ctron_list*)(t_bds))->items[t_bi]), t_anm);
}
}
}
}
t_bi += 1;
}
t_k += 1;
}
}
}
int t_interp_hit(const char* t_txt, const char* t_nm) 
{
int32_t t_nn = strlen((const char*)(t_nm));
if ((t_nn == 0)) {
{
return 0;
}
}
int32_t t_hn = strlen((const char*)(t_txt));
int32_t t_i = 0;
{
int t_ok = 0;
while (((t_i + t_nn) <= t_hn)) {
int32_t t_k = 0;
t_ok = 1;
while ((t_k < t_nn)) {
if ((ctron_byte_at(t_txt, (t_i + t_k)) != ctron_byte_at(t_nm, t_k))) {
{
t_ok = 0;
}
}
t_k += 1;
}
if (t_ok) {
{
return 1;
}
}
t_i += 1;
}
}
return 0;
}
void t_sem_calls_all(ctron_list* t_file, ctron_list* t_diags, const char* t_prof) 
{
ctron_list* t_fns = (ctron_list*)(ctron_list_new());
ctron_list* t_ars = (ctron_list*)(ctron_list_new());
ctron_list* t_rets = (ctron_list*)(ctron_list_new());
ctron_list* t_prms = (ctron_list*)(ctron_list_new());
ctron_list* t_stN = (ctron_list*)(ctron_list_new());
ctron_list* t_stT = (ctron_list*)(ctron_list_new());
ctron_list* t_vars = (ctron_list*)(ctron_list_new());
ctron_list* t_varAr = (ctron_list*)(ctron_list_new());
ctron_list* t_enums = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_d))->items[0]);
if (t_or2((strcmp((const char*)(t_t), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnPub")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Method")) == 0), (strcmp((const char*)(t_t), (const char*)("FnExt")) == 0)))))) {
{
ctron_list_push((ctron_list*)(t_fns), ((const char*)((ctron_list*)(t_d))->items[1]));
ctron_list_push((ctron_list*)(t_ars), ctron_i32_to_string((int32_t)((ctron_len((const void*)(((const char*)((ctron_list*)(t_d))->items[3]))) - 1))));
ctron_list_push((ctron_list*)(t_rets), ((const char*)((ctron_list*)(t_d))->items[4]));
ctron_list_push((ctron_list*)(t_prms), ((const char*)((ctron_list*)(t_d))->items[3]));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Static")) == 0)) {
{
ctron_list_push((ctron_list*)(t_stN), ((const char*)((ctron_list*)(t_d))->items[1]));
ctron_list_push((ctron_list*)(t_stT), ((const char*)((ctron_list*)(t_d))->items[3]));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Const")) == 0)) {
{
ctron_list_push((ctron_list*)(t_stN), ((const char*)((ctron_list*)(t_d))->items[1]));
ctron_list_push((ctron_list*)(t_stT), ((const char*)((ctron_list*)(t_d))->items[2]));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Enum")) == 0)) {
{
ctron_list_push((ctron_list*)(t_enums), ((const char*)((ctron_list*)(t_d))->items[1]));
const char* t_vs = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_j = 1;
{
const char* t_v = 0;
while ((t_j < ctron_len((const void*)(t_vs)))) {
t_v = ((const char*)((ctron_list*)(t_vs))->items[t_j]);
ctron_list_push((ctron_list*)(t_vars), ((const char*)((ctron_list*)(t_v))->items[1]));
ctron_list_push((ctron_list*)(t_varAr), ctron_i32_to_string((int32_t)((ctron_len((const void*)(((const char*)((ctron_list*)(t_v))->items[2]))) - 1))));
t_j += 1;
}
}
}
}
}
}
}
t_i += 1;
}
}
ctron_list* t_tpars = (ctron_list*)(ctron_list_new());
int32_t t_i0 = 1;
{
const char* t_d0 = 0;
while ((t_i0 < ((ctron_list*)(t_file))->n)) {
t_d0 = ((const char*)((ctron_list*)(t_file))->items[t_i0]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d0))->items[0])), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d0))->items[0])), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d0))->items[0])), (const char*)("FnPub")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d0))->items[0])), (const char*)("Method")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d0))->items[0])), (const char*)("Class")) == 0)))))) {
{
const char* t_tps = ((const char*)((ctron_list*)(t_d0))->items[2]);
int32_t t_q0 = 1;
while ((t_q0 < ctron_len((const void*)(t_tps)))) {
ctron_list_push((ctron_list*)(t_tpars), ctron_str_concat((const char*)("n:"), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tps))->items[t_q0])))->items[1]))));
t_q0 += 1;
}
}
}
t_i0 += 1;
}
}
ctron_list* t_prel = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_prel), "print");
ctron_list_push((ctron_list*)(t_prel), "println");
ctron_list_push((ctron_list*)(t_prel), "panic");
ctron_list_push((ctron_list*)(t_prel), "assert");
ctron_list_push((ctron_list*)(t_prel), "assert_eq");
ctron_list_push((ctron_list*)(t_prel), "assert_ne");
ctron_list_push((ctron_list*)(t_prel), "read_file");
ctron_list_push((ctron_list*)(t_prel), "read_dir");
ctron_list_push((ctron_list*)(t_prel), "fs_exists");
ctron_list_push((ctron_list*)(t_prel), "fs_write");
ctron_list_push((ctron_list*)(t_prel), "fs_delete");
ctron_list_push((ctron_list*)(t_prel), "now_ms");
ctron_list_push((ctron_list*)(t_prel), "now_ms_text");
ctron_list_push((ctron_list*)(t_prel), "byte_at");
ctron_list_push((ctron_list*)(t_prel), "byte_slice");
ctron_list_push((ctron_list*)(t_prel), "str_from_c");
ctron_list_push((ctron_list*)(t_prel), "clo_handle");
ctron_list_push((ctron_list*)(t_prel), "errno");
ctron_list_push((ctron_list*)(t_prel), "clo_cb2");
ctron_list_push((ctron_list*)(t_prel), "clo_cb3");
ctron_list_push((ctron_list*)(t_prel), "dlopen");
ctron_list_push((ctron_list*)(t_prel), "dlclose");
ctron_list_push((ctron_list*)(t_prel), "dlsym");
ctron_list_push((ctron_list*)(t_prel), "Some");
ctron_list_push((ctron_list*)(t_prel), "None");
ctron_list_push((ctron_list*)(t_prel), "Ok");
ctron_list_push((ctron_list*)(t_prel), "Err");
ctron_list_push((ctron_list*)(t_prel), "fmt");
ctron_list_push((ctron_list*)(t_prel), "char_len");
ctron_list_push((ctron_list*)(t_prel), "Box");
ctron_list_push((ctron_list*)(t_prel), "List");
ctron_list_push((ctron_list*)(t_prel), "StringBuilder");
ctron_list_push((ctron_list*)(t_prel), "String");
ctron_list_push((ctron_list*)(t_prel), "Channel");
ctron_list_push((ctron_list*)(t_prel), "Mutex");
ctron_list_push((ctron_list*)(t_prel), "Atomic");
ctron_list_push((ctron_list*)(t_prel), "Global");
ctron_list_push((ctron_list*)(t_prel), "Task");
ctron_list_push((ctron_list*)(t_prel), "Scope");
ctron_list_push((ctron_list*)(t_prel), "Simd");
ctron_list_push((ctron_list*)(t_prel), "Option");
ctron_list_push((ctron_list*)(t_prel), "Result");
ctron_list_push((ctron_list*)(t_prel), "AnyError");
ctron_list_push((ctron_list*)(t_prel), "Show");
ctron_list_push((ctron_list*)(t_prel), "Eq");
ctron_list_push((ctron_list*)(t_prel), "Error");
ctron_list_push((ctron_list*)(t_prel), "Cap");
ctron_list_push((ctron_list*)(t_prel), "Clone");
ctron_list_push((ctron_list*)(t_prel), "Hash");
ctron_list_push((ctron_list*)(t_prel), "Iter");
ctron_list_push((ctron_list*)(t_prel), "parallel");
ctron_list_push((ctron_list*)(t_prel), "bit");
ctron_list_push((ctron_list*)(t_prel), "arena");
if (t_seq2(t_prof, "web")) {
{
ctron_list_push((ctron_list*)(t_vars), "dom");
}
}
ctron_list* t_lets = (ctron_list*)(ctron_list_new());
ctron_list* t_reads = (ctron_list*)(ctron_list_new());
ctron_list* t_isrc = (ctron_list*)(ctron_list_new());
ctron_list* t_tc = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tc), (char*)(t_fns));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_ars));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_rets));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_prms));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_stN));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_stT));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_vars));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_varAr));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_enums));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_lets));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_reads));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_isrc));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_prel));
ctron_list_push((ctron_list*)(t_tc), (char*)(t_tpars));
t_i = 1;
{
const char* t_d = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_d))->items[0]);
if (t_or2((strcmp((const char*)(t_t), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("FnPub")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Method")) == 0), (strcmp((const char*)(t_t), (const char*)("FnExt")) == 0)))))) {
{
ctron_list* t_loc = (ctron_list*)(ctron_list_new());
ctron_list* t_envT = (ctron_list*)(ctron_list_new());
t_fn_params_push((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])), t_loc, t_envT);
ctron_list* t_wlets = (ctron_list*)(ctron_list_new());
ctron_list* t_wreads = (ctron_list*)(ctron_list_new());
ctron_list* t_wisrc = (ctron_list*)(ctron_list_new());
ctron_list* t_tcw = (ctron_list*)(ctron_list_new());
int32_t t_q = 0;
while ((t_q < 9)) {
ctron_list_push((ctron_list*)(t_tcw), ((const char*)((ctron_list*)(t_tc))->items[t_q]));
t_q += 1;
}
ctron_list_push((ctron_list*)(t_tcw), (char*)(t_wlets));
ctron_list_push((ctron_list*)(t_tcw), (char*)(t_wreads));
ctron_list_push((ctron_list*)(t_tcw), (char*)(t_wisrc));
ctron_list_push((ctron_list*)(t_tcw), (char*)(t_prel));
ctron_list_push((ctron_list*)(t_tcw), (char*)(t_tpars));
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_tcw, t_loc, t_envT, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[4])), t_diags);
int32_t t_u = 0;
{
const char* t_un = 0;
while ((t_u < ((ctron_list*)(t_wlets))->n)) {
t_un = ((const char*)((ctron_list*)(t_wlets))->items[t_u]);
if ((!t_in_list(t_wreads, t_un))) {
{
int t_hit = 0;
int32_t t_w = 0;
while (((t_w < ((ctron_list*)(t_wisrc))->n) && (!t_hit))) {
if (t_interp_hit(((const char*)((ctron_list*)(t_wisrc))->items[t_w]), t_un)) {
{
t_hit = 1;
}
}
t_w += 1;
}
if ((!t_hit)) {
{
int t_isu = 0;
if (((ctron_len((const void*)(t_un)) > 0) && (ctron_byte_at(t_un, 0) == 95))) {
{
t_isu = 1;
}
}
if ((!t_isu)) {
{
t_diag1(t_diags, "W8030", "", t_un);
}
}
}
}
}
}
t_u += 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Impl")) == 0)) {
{
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_mi = 1;
{
const char* t_md = 0;
while ((t_mi < ctron_len((const void*)(t_its)))) {
t_md = ((const char*)((ctron_list*)(t_its))->items[t_mi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Method")) == 0)) {
{
ctron_list* t_mloc = (ctron_list*)(ctron_list_new());
ctron_list* t_menv = (ctron_list*)(ctron_list_new());
t_fn_params_push((ctron_list*)(((const char*)((ctron_list*)(t_md))->items[3])), t_mloc, t_menv);
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_md))->items[5])), t_tc, t_mloc, t_menv, (ctron_list*)(((const char*)((ctron_list*)(t_md))->items[4])), t_diags);
}
}
t_mi += 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Test")) == 0)) {
{
t_tcb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), t_tc, ctron_list_new(), ctron_list_new(), ctron_list_new(), t_diags);
}
}
}
}
if ((strcmp((const char*)(t_t), (const char*)("Fn")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), 0, 0, 0, ctron_list_new(), t_diags);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Impl")) == 0)) {
{
const char* t_bits = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_bmi = 1;
{
const char* t_bmd = 0;
while ((t_bmi < ctron_len((const void*)(t_bits)))) {
t_bmd = ((const char*)((ctron_list*)(t_bits))->items[t_bmi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_bmd))->items[0])), (const char*)("Method")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_bmd))->items[5])), 0, 0, 0, ctron_list_new(), t_diags);
}
}
t_bmi += 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Test")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), 0, 0, 0, ctron_list_new(), t_diags);
}
}
}
}
t_i += 1;
}
}
}
void t_chk_break(ctron_list* t_file, ctron_list* t_n, int32_t t_depth, int32_t t_loopbase, int32_t t_outer, ctron_list* t_drops, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_n))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Break")) == 0)) {
{
if ((t_depth == 0)) {
{
if ((t_outer > 0)) {
{
t_diag0(t_diags, "E2072.brk", t_nline(t_n));
}
}
else {
{
t_diag0(t_diags, "E2070.brk", t_nline(t_n));
}
}
}
}
else {
if ((((ctron_list*)(t_drops))->n > t_loopbase)) {
{
t_diag0(t_diags, "E2071.brk", t_nline(t_n));
}
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Continue")) == 0)) {
{
if ((t_depth == 0)) {
{
if ((t_outer > 0)) {
{
t_diag0(t_diags, "E2072.cont", t_nline(t_n));
}
}
else {
{
t_diag0(t_diags, "E2070.cont", t_nline(t_n));
}
}
}
}
else {
if ((((ctron_list*)(t_drops))->n > t_loopbase)) {
{
t_diag0(t_diags, "E2071.cont", t_nline(t_n));
}
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
int32_t t_nouter = t_outer;
if ((t_depth > 0)) {
{
t_nouter = 1;
}
}
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[3])), 0, t_loopbase, t_nouter, t_drops, t_diags);
return;
}
}
int32_t t_nd = t_depth;
if (t_or2((strcmp((const char*)(t_t), (const char*)("While")) == 0), (strcmp((const char*)(t_t), (const char*)("For")) == 0))) {
{
t_nd = (t_depth + 1);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Block")) == 0)) {
{
ctron_list* t_cur = (ctron_list*)(ctron_list_new());
int32_t t_ci = 0;
while ((t_ci < ((ctron_list*)(t_drops))->n)) {
ctron_list_push((ctron_list*)(t_cur), ((const char*)((ctron_list*)(t_drops))->items[t_ci]));
t_ci += 1;
}
int32_t t_i = 1;
while ((t_i < (((ctron_list*)(t_n))->n - 1))) {
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[t_i])), t_nd, t_loopbase, t_outer, t_cur, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
if (((ctron_len((const void*)(((const char*)((ctron_list*)(t_n))->items[3]))) > 1) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_n))->items[3])))->items[0])), (const char*)("Named")) == 0))) {
{
if (t_impl_has_drop(t_file, ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_n))->items[3])))->items[1]))) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_n))->items[2])))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_drops), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_n))->items[2])))->items[1]));
}
}
}
}
}
}
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_n))->items[4]))) > 1)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[4])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[1])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
int32_t t_lb = ((ctron_list*)(t_drops))->n;
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[2])), t_nd, t_lb, t_outer, t_drops, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
int32_t t_lb = ((ctron_list*)(t_drops))->n;
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[3])), t_nd, t_lb, t_outer, t_drops, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[2])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
const char* t_els = ((const char*)((ctron_list*)(t_n))->items[3]);
if ((ctron_len((const void*)(t_els)) > 1)) {
{
t_chk_break(t_file, (ctron_list*)(t_els), t_nd, t_loopbase, t_outer, t_drops, t_diags);
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
const char* t_arms = ((const char*)((ctron_list*)(t_n))->items[2]);
int32_t t_ai = 1;
{
const char* t_arm = 0;
while ((t_ai < ctron_len((const void*)(t_arms)))) {
t_arm = ((const char*)((ctron_list*)(t_arms))->items[t_ai]);
if ((ctron_len((const void*)(t_arm)) > 2)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_arm))->items[2])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
}
}
t_ai += 1;
}
}
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[2])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[1])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_n))->items[1]))) > 1)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[1])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[1])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
t_chk_break(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_n))->items[3])), t_nd, t_loopbase, t_outer, t_drops, t_diags);
return;
}
}
}
int t_impl_has_drop(ctron_list* t_file, const char* t_tyname) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_d))->items[2]))) > 1)) && (ctron_len((const void*)(((const char*)((ctron_list*)(t_d))->items[3]))) > 1))) {
{
if ((t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])))->items[1]), "Drop") && t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])))->items[1]), t_tyname))) {
{
return 1;
}
}
}
}
t_i += 1;
}
}
return 0;
}
int t_comp_banned_call(const char* t_nm) 
{
return t_or2(t_seq2(t_nm, "print"), t_or2(t_seq2(t_nm, "println"), t_or2(t_seq2(t_nm, "read_file"), t_seq2(t_nm, "read_dir"))));
}
int t_comp_banned_mem(const char* t_nm) 
{
return t_or2(t_seq2(t_nm, "spawn"), t_or2(t_seq2(t_nm, "send"), t_or2(t_seq2(t_nm, "recv"), t_or2(t_seq2(t_nm, "store"), t_or2(t_seq2(t_nm, "fetch_add"), t_or2(t_seq2(t_nm, "with"), t_seq2(t_nm, "with_mut")))))));
}
void t_scan_comp(ctron_list* t_file, ctron_list* t_e, ctron_list* t_diags, const char* t_fnm) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
const char* t_rt = t_root_name((ctron_list*)(t_cal));
if (t_comp_banned_call(t_rt)) {
{
t_donce2(t_diags, "E6020.effect", t_nline(t_e), t_fnm, t_rt);
}
}
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[2])), (const char*)("Nm")) == 0)) && t_comp_banned_mem(((const char*)((ctron_list*)(t_cal))->items[3])))) {
{
t_donce2(t_diags, "E6020.effect", t_nline(t_e), t_fnm, ctron_str_concat((const char*)("."), (const char*)(((const char*)((ctron_list*)(t_cal))->items[3]))));
}
}
t_scan_comp(t_file, (ctron_list*)(t_cal), t_diags, t_fnm);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_diags, t_fnm);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_diags, t_fnm);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_diags, t_fnm);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_diags, t_fnm);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_diags, t_fnm);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_diags, t_fnm);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i4 = 1;
{
const char* t_c = 0;
while ((t_i4 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i4]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_diags, t_fnm);
}
}
t_i4 += 1;
}
}
return;
}
}
}
void t_scb(ctron_list* t_file, ctron_list* t_b, ctron_list* t_diags, const char* t_fnm) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_diags, t_fnm);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_diags, t_fnm);
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_diags, t_fnm);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_diags, t_fnm);
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_diags, t_fnm);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_st))->items[1]))) > 1)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_diags, t_fnm);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_diags, t_fnm);
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_diags, t_fnm);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_scan_comp(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_diags, t_fnm);
}
}
else {
if ((t_seq2(t_t, "None") && (ctron_len((const void*)(t_st)) == 1))) {
{
}
}
else {
{
t_scan_comp(t_file, (ctron_list*)(t_st), t_diags, t_fnm);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
int t_vtag_ok(const char* t_k, const char* t_vt) 
{
if (t_k_int(t_k)) {
{
if (t_seq2(t_k, "n:I64")) {
{
if (t_seq2(t_vt, "6")) {
{
return 1;
}
}
}
}
return t_or2(t_seq2(t_vt, "I"), t_seq2(t_vt, "W"));
}
}
if (t_or2(t_seq2(t_k, "n:Str"), t_seq2(t_k, "n:String"))) {
{
return t_seq2(t_vt, "S");
}
}
if (t_seq2(t_k, "n:Bool")) {
{
return t_seq2(t_vt, "B");
}
}
if (t_or2(t_seq2(t_k, "n:F32"), t_seq2(t_k, "n:F64"))) {
{
return t_seq2(t_vt, "D");
}
}
if (t_seq2(t_k, "void")) {
{
return t_seq2(t_vt, "V");
}
}
return 1;
}
void t_sem_comp(ctron_list* t_file, ctron_list* t_diags) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0)) {
{
const char* t_tn = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])));
const char* t_tyn = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])));
if (((strlen((const char*)(t_tn)) > 0) && (strlen((const char*)(t_tyn)) > 0))) {
{
if (((!t_local_type_or_trait(t_file, t_tn)) && (!t_local_type_or_trait(t_file, t_tyn)))) {
{
t_diag2(t_diags, "E5010", "", t_tn, t_tyn);
}
}
}
}
}
}
t_i += 1;
}
}
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return;
}
}
t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnC")) == 0)) {
{
t_scb(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_diags, ((const char*)((ctron_list*)(t_d))->items[1]));
}
}
t_i += 1;
}
}
if ((((ctron_list*)(t_diags))->n > 0)) {
{
return;
}
}
ctron_list* t_env = (ctron_list*)(ctron_list_new());
t_i = 1;
{
const char* t_d = 0;
const char* t_nm = 0;
const char* t_knd = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_nm = "";
ctron_list* t_expr = (ctron_list*)(ctron_list_new());
t_knd = "";
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Const")) == 0)) {
{
t_nm = ((const char*)((ctron_list*)(t_d))->items[1]);
t_expr = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]));
t_knd = t_ty_kind((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])));
}
}
else {
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Static")) == 0) && t_seq2(((const char*)((ctron_list*)(t_d))->items[2]), "false"))) {
{
t_nm = ((const char*)((ctron_list*)(t_d))->items[1]);
t_expr = (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[4]));
t_knd = t_ty_kind((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])));
}
}
}
if ((strlen((const char*)(t_nm)) > 0)) {
{
ctron_list* t_st = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_st), "0");
ctron_list* t_cenvN = (ctron_list*)(ctron_list_new());
ctron_list* t_cenvV = (ctron_list*)(ctron_list_new());
ctron_list* t_vr = (ctron_list*)(t_ceval_expr(t_file, t_expr, t_cenvN, t_cenvV, t_st));
if (t_seq2(((const char*)((ctron_list*)(t_vr))->items[0]), "over")) {
{
t_diag1(t_diags, "E6010", "", t_nm);
return;
}
}
int t_done = 0;
if (t_seq2(((const char*)((ctron_list*)(t_vr))->items[0]), "k")) {
{
t_Val t_rv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vr))->items[2])));
if (((strlen((const char*)(t_knd)) > 0) && (!t_vtag_ok(t_knd, t_vtag_str(t_rv))))) {
{
t_diag3(t_diags, "E2010.const", "", t_nm, t_knd, t_vtag_str(t_rv));
return;
}
}
if ((((t_rv).tag == 2) && (!t_c6_in_i64(t_v6t(t_rv))))) {
{
t_diag1(t_diags, "E2040.const", "", t_nm);
return;
}
}
t_env = (ctron_list*)(t_env_add(t_env, t_nm, t_rv));
t_done = 1;
}
}
if ((!t_done)) {
{
t_Flow t_vr2 = t_eval_expr(t_file, t_env, "", t_expr);
if ((strcmp((const char*)((t_vr2).kind), (const char*)("k")) != 0)) {
{
return;
}
}
if (((strlen((const char*)(t_knd)) > 0) && (!t_vtag_ok(t_knd, t_vtag_str((t_vr2).v))))) {
{
t_diag3(t_diags, "E2010.const", "", t_nm, t_knd, t_vtag_str((t_vr2).v));
return;
}
}
if (((((t_vr2).v).tag == 2) && (!t_c6_in_i64(t_v6t((t_vr2).v))))) {
{
t_diag1(t_diags, "E2040.const", "", t_nm);
return;
}
}
t_env = (ctron_list*)(t_env_add(t_env, t_nm, (t_vr2).v));
}
}
}
}
t_i += 1;
}
}
}
int t_local_type_or_trait(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_d))->items[0]);
if (t_or2((strcmp((const char*)(t_t), (const char*)("Trait")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Struct")) == 0), t_or2((strcmp((const char*)(t_t), (const char*)("Enum")) == 0), (strcmp((const char*)(t_t), (const char*)("Class")) == 0))))) {
{
if (t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm)) {
{
return 1;
}
}
}
}
t_i += 1;
}
}
return 0;
}
int t_ce_budget(ctron_list* t_st) 
{
int32_t t_used = t_dvi(((const char*)((ctron_list*)(t_st))->items[0]));
if ((t_used > 1200)) {
{
return 0;
}
}
((ctron_list*)(t_st))->items[0] = ctron_i32_to_string((int32_t)((t_used + 1)));
return 1;
}
ctron_list* t_ceval_expr(ctron_list* t_file, ctron_list* t_e, ctron_list* t_cenvN, ctron_list* t_cenvV, ctron_list* t_st) 
{
if ((!t_ce_budget(t_st))) {
{
return t_cover();
}
}
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Int")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 0)) {
{
if (t_lit_wide_i32(((const char*)((ctron_list*)(t_e))->items[1]))) {
{
return t_cok(t_v6(t_lit_to_dec(((const char*)((ctron_list*)(t_e))->items[1]))));
}
}
}
}
return t_cok(t_vI(t_dvi(t_num_text(((const char*)((ctron_list*)(t_e))->items[1])))));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Str")) == 0)) {
{
const char* t_parts = ((const char*)((ctron_list*)(t_e))->items[1]);
const char* t_pld = "";
int t_okp = 1;
int32_t t_i5 = 1;
{
const char* t_pp = 0;
while ((t_i5 < ctron_len((const void*)(t_parts)))) {
t_pp = ((const char*)((ctron_list*)(t_parts))->items[t_i5]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pp))->items[0])), (const char*)("Text")) == 0)) {
{
t_pld = ctron_str_concat((const char*)(t_pld), (const char*)(t_unesc(((const char*)((ctron_list*)(t_pp))->items[1]))));
}
}
else {
{
t_okp = 0;
}
}
t_i5 += 1;
}
}
if (t_okp) {
{
return t_cok(t_vS(t_pld));
}
}
return t_cun();
}
}
if ((strcmp((const char*)(t_t), (const char*)("Float")) == 0)) {
{
return t_cok(t_vD(t_df_can(((const char*)((ctron_list*)(t_e))->items[1]))));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
int32_t t_i = (((ctron_list*)(t_cenvN))->n - 1);
while ((t_i >= 0)) {
if (t_seq2(((const char*)((ctron_list*)(t_cenvN))->items[t_i]), ((const char*)((ctron_list*)(t_e))->items[1]))) {
{
return t_cok(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cenvV))->items[t_i]))));
}
}
t_i -= 1;
}
return t_cun();
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
ctron_list* t_x = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_x))->items[0])), (const char*)("k")) != 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_x))->items[0]), "over")) {
{
return t_x;
}
}
return t_x;
}
}
t_Val t_xu = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_x))->items[2])));
if (((t_xu).tag != 1)) {
{
if (((t_xu).tag == 2)) {
{
return t_cok(t_v6(t_c6can(ctron_str_concat((const char*)("-"), (const char*)(t_v6t(t_xu))))));
}
}
return t_cun();
}
}
return t_cok(t_vI((0 - (int32_t)((t_xu).iv))));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_op = ((const char*)((ctron_list*)(t_e))->items[1]);
if (t_seq2(t_op, "AndAnd")) {
{
return t_cun();
}
}
if (t_seq2(t_op, "OrOr")) {
{
return t_cun();
}
}
ctron_list* t_l = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_l))->items[0])), (const char*)("k")) != 0)) {
{
return t_l;
}
}
ctron_list* t_r = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_r))->items[0])), (const char*)("k")) != 0)) {
{
return t_r;
}
}
t_Val t_lu = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_l))->items[2])));
t_Val t_ru = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_r))->items[2])));
if (t_or2(((t_lu).tag == 2), ((t_ru).tag == 2))) {
{
if (t_is_cmp_op(t_op)) {
{
int32_t t_cv = t_vcmp(t_lu, t_ru);
int t_rb = 0;
if (t_seq2(t_op, "Lt")) {
{
t_rb = (t_cv < 0);
}
}
if (t_seq2(t_op, "Gt")) {
{
t_rb = (t_cv > 0);
}
}
if (t_seq2(t_op, "Le")) {
{
t_rb = (t_cv <= 0);
}
}
if (t_seq2(t_op, "Ge")) {
{
t_rb = (t_cv >= 0);
}
}
if (t_seq2(t_op, "Eq")) {
{
t_rb = (t_cv == 0);
}
}
if (t_seq2(t_op, "Ne")) {
{
t_rb = (t_cv != 0);
}
}
return t_cok(t_vB(t_rb));
}
}
if (t_or2(t_seq2(t_op, "Add"), t_or2(t_seq2(t_op, "Sub"), t_or2(t_seq2(t_op, "Mul"), t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "Mod")))))) {
{
return t_cok(t_val_arith(t_op, t_lu, t_ru));
}
}
return t_cun();
}
}
if (t_or2(((t_lu).tag != 1), ((t_ru).tag != 1))) {
{
return t_cun();
}
}
int32_t t_a = (int32_t)((t_lu).iv);
int32_t t_b2 = (int32_t)((t_ru).iv);
if (t_is_cmp_op(t_op)) {
{
int t_rb = 0;
if (t_seq2(t_op, "Lt")) {
{
t_rb = (t_a < t_b2);
}
}
if (t_seq2(t_op, "Gt")) {
{
t_rb = (t_a > t_b2);
}
}
if (t_seq2(t_op, "Le")) {
{
t_rb = (t_a <= t_b2);
}
}
if (t_seq2(t_op, "Ge")) {
{
t_rb = (t_a >= t_b2);
}
}
if (t_seq2(t_op, "Eq")) {
{
t_rb = (t_a == t_b2);
}
}
if (t_seq2(t_op, "Ne")) {
{
t_rb = (t_a != t_b2);
}
}
return t_cok(t_vB(t_rb));
}
}
if (t_seq2(t_op, "Add")) {
{
return t_cok(t_vI((t_a + t_b2)));
}
}
if (t_seq2(t_op, "Sub")) {
{
return t_cok(t_vI((t_a - t_b2)));
}
}
if (t_seq2(t_op, "Mul")) {
{
return t_cok(t_vI((t_a * t_b2)));
}
}
if (t_seq2(t_op, "Div")) {
{
if ((t_b2 == 0)) {
{
return t_cun();
}
}
return t_cok(t_vI((t_a / t_b2)));
}
}
return t_cun();
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) != 0)) {
{
return t_cun();
}
}
const char* t_fnm = ((const char*)((ctron_list*)(t_cal))->items[1]);
ctron_list* t_d = (ctron_list*)(t_find_decl(t_file, t_fnm));
int t_known = (((ctron_list*)(t_d))->n > 0);
if (t_known) {
{
t_known = (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnC")) == 0);
}
}
if ((!t_known)) {
{
return t_cun();
}
}
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
ctron_list* t_cenvN2 = (ctron_list*)(ctron_list_new());
ctron_list* t_cenvV2 = (ctron_list*)(ctron_list_new());
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_i = 1;
int32_t t_ai = 1;
{
const char* t_pr = 0;
while ((t_i < ctron_len((const void*)(t_ps)))) {
t_pr = ((const char*)((ctron_list*)(t_ps))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pr))->items[0])), (const char*)("Param")) == 0)) {
{
if ((t_ai >= ctron_len((const void*)(t_ag)))) {
{
return t_cun();
}
}
ctron_list* t_av = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_ai])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_av))->items[0])), (const char*)("k")) != 0)) {
{
return t_av;
}
}
if (((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_av))->items[2])))).tag != 1)) {
{
return t_cun();
}
}
ctron_list_push((ctron_list*)(t_cenvN2), ((const char*)((ctron_list*)(t_pr))->items[2]));
ctron_list_push((ctron_list*)(t_cenvV2), ((const char*)((ctron_list*)(t_av))->items[2]));
}
}
else {
{
return t_cun();
}
}
t_i += 1;
t_ai += 1;
}
}
return t_ceval_block(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_cenvN2, t_cenvV2, t_st);
}
}
return t_cun();
}
ctron_list* t_ceval_block(ctron_list* t_file, ctron_list* t_b, ctron_list* t_cenvN, ctron_list* t_cenvV, ctron_list* t_st) 
{
int32_t t_n = ((ctron_list*)(t_b))->n;
int32_t t_i = 1;
{
const char* t_st2 = 0;
const char* t_t = 0;
while ((t_i < (t_n - 1))) {
t_st2 = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st2))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_st2))->items[1]))) > 1)) {
{
return t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[1])), t_cenvN, t_cenvV, t_st);
}
}
return t_cun();
}
}
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_p7 = ((const char*)((ctron_list*)(t_st2))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p7))->items[0])), (const char*)("PatId")) != 0)) {
{
return t_cun();
}
}
ctron_list* t_v7 = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[4])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_v7))->items[0])), (const char*)("k")) != 0)) {
{
return t_v7;
}
}
if (((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_v7))->items[2])))).tag != 1)) {
{
return t_cun();
}
}
ctron_list_push((ctron_list*)(t_cenvN), ((const char*)((ctron_list*)(t_p7))->items[1]));
ctron_list_push((ctron_list*)(t_cenvV), ((const char*)((ctron_list*)(t_v7))->items[2]));
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
const char* t_tgt7 = ((const char*)((ctron_list*)(t_st2))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tgt7))->items[0])), (const char*)("Ident")) != 0)) {
{
return t_cun();
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_st2))->items[2])), (const char*)("Eq")) != 0)) {
{
return t_cun();
}
}
ctron_list* t_v7 = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[3])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_v7))->items[0])), (const char*)("k")) != 0)) {
{
return t_v7;
}
}
if (((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_v7))->items[2])))).tag != 1)) {
{
return t_cun();
}
}
int32_t t_f7 = (-1);
int32_t t_k7 = 0;
while ((t_k7 < ((ctron_list*)(t_cenvN))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_cenvN))->items[t_k7]), ((const char*)((ctron_list*)(t_tgt7))->items[1]))) {
{
t_f7 = t_k7;
}
}
t_k7 += 1;
}
if ((t_f7 < 0)) {
{
return t_cun();
}
}
((ctron_list*)(t_cenvV))->items[t_f7] = ((const char*)((ctron_list*)(t_v7))->items[2]);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
int t_g7 = 1;
while (t_g7) {
if ((!t_ce_budget(t_st))) {
{
return t_cover();
}
}
ctron_list* t_cv = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[1])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cv))->items[0])), (const char*)("k")) != 0)) {
{
return t_cv;
}
}
if ((!t_truth(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cv))->items[2])))))) {
{
t_g7 = 0;
}
}
else {
{
const char* t_wb7 = ((const char*)((ctron_list*)(t_st2))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_wb7))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_wb7 = ((const char*)((ctron_list*)(t_wb7))->items[1]);
}
}
ctron_list* t_br7 = (ctron_list*)(t_ceval_block(t_file, (ctron_list*)(t_wb7), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_br7))->items[0])), (const char*)("over")) == 0)) {
{
return t_br7;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_br7))->items[0])), (const char*)("k")) == 0)) {
{
return t_br7;
}
}
}
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
const char* t_pat8 = ((const char*)((ctron_list*)(t_st2))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat8))->items[0])), (const char*)("PatId")) != 0)) {
{
return t_cun();
}
}
const char* t_it8 = ((const char*)((ctron_list*)(t_st2))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_it8))->items[0])), (const char*)("Range")) != 0)) {
{
return t_cun();
}
}
ctron_list* t_lov = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_it8))->items[2])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_lov))->items[0])), (const char*)("k")) != 0)) {
{
return t_lov;
}
}
ctron_list* t_hiv = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_it8))->items[3])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_hiv))->items[0])), (const char*)("k")) != 0)) {
{
return t_hiv;
}
}
int32_t t_lo8 = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_lov))->items[2])))).iv);
int32_t t_hi8 = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_hiv))->items[2])))).iv);
if (t_seq2(((const char*)((ctron_list*)(t_it8))->items[1]), "true")) {
{
t_hi8 += 1;
}
}
int32_t t_k8 = t_lo8;
{
const char* t_body8 = 0;
while ((t_k8 < t_hi8)) {
if ((!t_ce_budget(t_st))) {
{
return t_cover();
}
}
ctron_list_push((ctron_list*)(t_cenvN), ((const char*)((ctron_list*)(t_pat8))->items[1]));
ctron_list_push((ctron_list*)(t_cenvV), (char*)(t_v_box(t_vI(t_k8))));
t_body8 = ((const char*)((ctron_list*)(t_st2))->items[3]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_body8))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_body8 = ((const char*)((ctron_list*)(t_body8))->items[1]);
}
}
ctron_list* t_br8 = (ctron_list*)(t_ceval_block(t_file, (ctron_list*)(t_body8), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_br8))->items[0])), (const char*)("over")) == 0)) {
{
return t_br8;
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_br8))->items[0])), (const char*)("k")) == 0)) {
{
return t_br8;
}
}
t_k8 += 1;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Break")) == 0)) {
{
return t_cun();
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Continue")) == 0)) {
{
return t_cun();
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
ctron_list* t_v = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[1])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_v))->items[0])), (const char*)("k")) != 0)) {
{
return t_v;
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
ctron_list* t_cv = (ctron_list*)(t_ceval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st2))->items[1])), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cv))->items[0])), (const char*)("k")) != 0)) {
{
return t_cv;
}
}
const char* t_target = ((const char*)((ctron_list*)(t_st2))->items[2]);
if ((!t_truth(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cv))->items[2])))))) {
{
t_target = ((const char*)((ctron_list*)(t_st2))->items[3]);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_target))->items[0])), (const char*)("BlockExpr")) == 0)) {
{
t_target = ((const char*)((ctron_list*)(t_target))->items[1]);
}
}
ctron_list* t_br = (ctron_list*)(t_ceval_block(t_file, (ctron_list*)(t_target), t_cenvN, t_cenvV, t_st));
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_br))->items[0])), (const char*)("u")) != 0)) {
{
return t_br;
}
}
}
}
else {
{
return t_cun();
}
}
}
}
}
}
}
}
}
t_i += 1;
}
}
const char* t_tl = ((const char*)((ctron_list*)(t_b))->items[(t_n - 1)]);
if ((t_seq2(((const char*)((ctron_list*)(t_tl))->items[0]), "None") && (ctron_len((const void*)(t_tl)) == 1))) {
{
return t_cun();
}
}
return t_ceval_expr(t_file, (ctron_list*)(t_tl), t_cenvN, t_cenvV, t_st);
}
ctron_list* t_cok(t_Val t_v) 
{
ctron_list* t_r = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_r), "k");
ctron_list_push((ctron_list*)(t_r), "");
ctron_list_push((ctron_list*)(t_r), (char*)(t_v_box(t_v)));
return t_r;
}
ctron_list* t_cun() 
{
ctron_list* t_r = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_r), "u");
return t_r;
}
ctron_list* t_cover() 
{
ctron_list* t_r = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_r), "over");
return t_r;
}
void t_mblock(ctron_list* t_file, ctron_list* t_b, ctron_list* t_out) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_st))->items[1]), "true") && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])))->items[0])), (const char*)("PatId")) == 0))) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])))->items[1]));
}
}
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_out);
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_st))->items[1]))) > 1)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
}
}
else {
if ((t_seq2(t_t, "None") && (ctron_len((const void*)(t_st)) == 1))) {
{
}
}
else {
{
t_mexpr(t_file, (ctron_list*)(t_st), t_out);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
void t_mexpr(ctron_list* t_file, ctron_list* t_e, ctron_list* t_out) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i2 = 1;
while ((t_i2 < ctron_len((const void*)(t_arms)))) {
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i2])))->items[2])), t_out);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_ag)))) {
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i3])), t_out);
t_i3 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i4 = 1;
while ((t_i4 < ((ctron_list*)(t_e))->n)) {
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i4])), t_out);
t_i4 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i5 = 1;
{
const char* t_c = 0;
while ((t_i5 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i5]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_mexpr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_out);
}
}
t_i5 += 1;
}
}
return;
}
}
}
void t_mblock2(ctron_list* t_file, ctron_list* t_out) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnPub")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnExt")) == 0))))) {
{
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_out);
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0)) {
{
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_mi = 1;
{
const char* t_md = 0;
while ((t_mi < ctron_len((const void*)(t_its)))) {
t_md = ((const char*)((ctron_list*)(t_its))->items[t_mi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Method")) == 0)) {
{
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_md))->items[5])), t_out);
}
}
t_mi += 1;
}
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Test")) == 0)) {
{
t_mblock(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), t_out);
}
}
}
}
t_i += 1;
}
}
}
void t_cassign_names(ctron_list* t_e, ctron_list* t_out) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Block")) == 0)) {
{
t_cblock2(t_e, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_cblock2((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1]));
}
}
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_out);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_out);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr2")) == 0)) {
{
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_cblock2((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_out);
t_i3 += 1;
}
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_cblock2((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i4 = 1;
{
const char* t_c = 0;
while ((t_i4 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i4]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_out);
}
}
t_i4 += 1;
}
}
return;
}
}
}
void t_cblock2(ctron_list* t_b, ctron_list* t_out) 
{
int32_t t_i = 1;
{
const char* t_st = 0;
const char* t_t = 0;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_st = ((const char*)((ctron_list*)(t_b))->items[t_i]);
t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
t_cblock2((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), t_out);
t_cblock2((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_st))->items[1]))) > 1)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
}
}
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])))->items[1]));
}
}
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), t_out);
}
}
else {
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])), t_out);
}
}
else {
if ((t_seq2(t_t, "None") && (ctron_len((const void*)(t_st)) == 1))) {
{
}
}
else {
{
t_cassign_names((ctron_list*)(t_st), t_out);
}
}
}
}
}
}
}
}
t_i += 1;
}
}
}
void t_cscan(ctron_list* t_node, ctron_list* t_muts, ctron_list* t_diags) 
{
const char* t_t = ((const char*)((ctron_list*)(t_node))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
ctron_list* t_names = (ctron_list*)(ctron_list_new());
t_cassign_names((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_names);
ctron_list* t_locals = (ctron_list*)(ctron_list_new());
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_locals);
int32_t t_i = 0;
{
const char* t_nm = 0;
while ((t_i < ((ctron_list*)(t_names))->n)) {
t_nm = ((const char*)((ctron_list*)(t_names))->items[t_i]);
if (t_in_list(t_muts, t_nm)) {
{
if ((!t_in_list(t_locals, t_nm))) {
{
t_donce1(t_diags, "E3070", "", t_nm);
}
}
}
}
t_i += 1;
}
}
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
const char* t_ag = ((const char*)((ctron_list*)(t_node))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_muts, t_diags);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_node))->n)) {
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[t_i2])), t_muts, t_diags);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Block")) == 0)) {
{
t_cscan_b(t_node, t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[4])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_node))->items[1]))) > 1)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[3])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[1])), t_muts, t_diags);
const char* t_arms = ((const char*)((ctron_list*)(t_node))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_cscan((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_muts, t_diags);
t_i3 += 1;
}
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_node))->items[2])), t_muts, t_diags);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_node))->items[2]);
int32_t t_i4 = 1;
{
const char* t_c = 0;
while ((t_i4 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i4]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_muts, t_diags);
}
}
t_i4 += 1;
}
}
return;
}
}
}
void t_cscan_b(ctron_list* t_b, ctron_list* t_muts, ctron_list* t_diags) 
{
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_cscan((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])), t_muts, t_diags);
t_i += 1;
}
}
void t_sem_closures(ctron_list* t_file, ctron_list* t_diags) 
{
ctron_list* t_muts = (ctron_list*)(ctron_list_new());
t_mblock2(t_file, t_muts);
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Fn")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnC")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnPub")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("FnExt")) == 0))))) {
{
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), t_muts, t_diags);
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0)) {
{
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_mi = 1;
{
const char* t_md = 0;
while ((t_mi < ctron_len((const void*)(t_its)))) {
t_md = ((const char*)((ctron_list*)(t_its))->items[t_mi]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_md))->items[0])), (const char*)("Method")) == 0)) {
{
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_md))->items[5])), t_muts, t_diags);
}
}
t_mi += 1;
}
}
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Test")) == 0)) {
{
t_cscan_b((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), t_muts, t_diags);
}
}
}
}
t_i += 1;
}
}
}
void t_clet_b(ctron_list* t_b, ctron_list* t_out) 
{
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_b))->n)) {
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_b))->items[t_i])), t_out);
t_i += 1;
}
}
void t_clet_names(ctron_list* t_e, ctron_list* t_out) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Block")) == 0)) {
{
t_clet_b(t_e, t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_clet_b((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_pat = ((const char*)((ctron_list*)(t_e))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_pat))->items[1]));
}
}
else {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatTup")) == 0)) {
{
int32_t t_pi = 1;
while ((t_pi < ctron_len((const void*)(t_pat)))) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_pat))->items[t_pi])))->items[0])), (const char*)("PatId")) == 0)) {
{
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_pat))->items[t_pi])))->items[1]));
}
}
t_pi += 1;
}
}
}
}
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[4])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])), t_out);
t_i += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0), (strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0))) {
{
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_e))->n)) {
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_i2])), t_out);
t_i2 += 1;
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_clet_b((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i3 = 1;
while ((t_i3 < ctron_len((const void*)(t_arms)))) {
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_arms))->items[t_i3])))->items[2])), t_out);
t_i3 += 1;
}
return;
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("Own")) == 0), (strcmp((const char*)(t_t), (const char*)("Scope")) == 0))) {
{
t_clet_b((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i4 = 1;
{
const char* t_c = 0;
while ((t_i4 < ctron_len((const void*)(t_lf)))) {
t_c = ((const char*)((ctron_list*)(t_lf))->items[t_i4]);
if (t_seq2(((const char*)((ctron_list*)(t_c))->items[0]), "LField")) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_c))->items[2])), t_out);
}
}
t_i4 += 1;
}
}
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), t_out);
t_clet_b((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
return;
}
}
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
t_clet_names((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), t_out);
t_clet_b((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])), t_out);
return;
}
}
}
int32_t t_digit_of(int32_t t_c) 
{
if (((t_c >= 48) && (t_c <= 57))) {
{
return (t_c - 48);
}
}
if (((t_c >= 97) && (t_c <= 102))) {
{
return ((t_c - 97) + 10);
}
}
if (((t_c >= 65) && (t_c <= 70))) {
{
return ((t_c - 65) + 10);
}
}
return 0;
}
int32_t t_txt_num(const char* t_t) 
{
int32_t t_i = 0;
int32_t t_n = strlen((const char*)(t_t));
int32_t t_radix = 10;
if ((((t_n >= 2) && (ctron_byte_at(t_t, 0) == 48)) && (ctron_byte_at(t_t, 1) == 120))) {
{
t_radix = 16;
t_i = 2;
}
}
if ((((t_n >= 2) && (ctron_byte_at(t_t, 0) == 48)) && (ctron_byte_at(t_t, 1) == 111))) {
{
t_radix = 8;
t_i = 2;
}
}
if ((((t_n >= 2) && (ctron_byte_at(t_t, 0) == 48)) && (ctron_byte_at(t_t, 1) == 98))) {
{
t_radix = 2;
t_i = 2;
}
}
if (((t_radix == 10) && (t_i == 0))) {
{
int t_allc = 1;
int32_t t_j = 0;
{
int32_t t_c = 0;
while ((t_j < t_n)) {
t_c = ctron_byte_at(t_t, t_j);
if ((t_c == 95)) {
{
t_allc = 0;
}
}
if ((t_c < 48)) {
{
t_allc = 0;
}
}
if ((t_c > 57)) {
{
t_allc = 0;
}
}
if ((((t_j == 0) && (t_n > 1)) && (t_c == 48))) {
{
t_allc = 0;
}
}
t_j += 1;
}
}
if (t_allc) {
{
int64_t t_v = 0;
int32_t t_q = 0;
while ((t_q < t_n)) {
t_v = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_v), (int64_t)(10))), (int64_t)((ctron_byte_at(t_t, t_q) - 48)));
t_q += 1;
}
int64_t t_mo = ctron_i64_mod((int64_t)(t_v), (int64_t)(4294967296LL));
if ((t_mo >= 2147483648LL)) {
{
t_mo = ctron_i64_sub((int64_t)(t_mo), (int64_t)(4294967296LL));
}
}
return (int32_t)(t_mo);
}
}
}
}
const char* t_acc7 = "0";
const char* t_rtxt = t_dvi_str(t_radix);
{
int32_t t_c = 0;
while ((t_i < t_n)) {
t_c = ctron_byte_at(t_t, t_i);
if ((t_c == 95)) {
{
t_i += 1;
}
}
else {
{
t_acc7 = t_c6add(t_c6mul(t_acc7, t_rtxt), t_dvi_str(t_digit_of(t_c)));
t_i += 1;
}
}
}
}
return t_wrap_i32(t_acc7);
}
const char* t_dvi_str(int32_t t_v) 
{
return ctron_i32_to_string((int32_t)(t_v));
}
int32_t t_wrap_i32(const char* t_u) 
{
const char* t_m = t_c6can(t_u);
if ((strlen((const char*)(t_m)) > 0)) {
{
if ((ctron_byte_at(t_m, 0) == 45)) {
{
t_m = ctron_byte_slice(t_m, 1, strlen((const char*)(t_m)));
}
}
}
}
const char* t_qr = t_c6divmod(t_m, "4294967296");
int32_t t_bar = 0;
int32_t t_ci = 0;
while ((t_ci < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_ci) == 124)) {
{
t_bar = t_ci;
}
}
t_ci += 1;
}
const char* t_r = ctron_byte_slice(t_qr, (t_bar + 1), strlen((const char*)(t_qr)));
if ((t_c6abscmp(t_r, "2147483647") > 0)) {
{
t_r = t_c6sub(t_r, "4294967296");
}
}
return t_dvi(t_r);
}
ctron_list* t_cx_wrap(ctron_list* t_l) 
{
ctron_list* t_w = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_w), (char*)(t_l));
return t_w;
}
ctron_list* t_cx_of(t_Val t_v) 
{
return ((const char*)((ctron_list*)((t_v).cx))->items[0]);
}
t_Val t_vI(int32_t t_x) 
{
return (t_Val){.tag = 1, .iv = (int64_t)(t_x), .b = 0, .s = "", .cx = t_VNIL};
}
t_Val t_vB(int t_b) 
{
return (t_Val){.tag = 4, .iv = 0, .b = t_b, .s = "", .cx = t_VNIL};
}
t_Val t_vS(const char* t_s) 
{
return (t_Val){.tag = 5, .iv = 0, .b = 0, .s = t_s, .cx = t_VNIL};
}
t_Val t_vV2() 
{
return (t_Val){.tag = 0, .iv = 0, .b = 0, .s = "", .cx = t_VNIL};
}
t_Val t_v6(const char* t_t) 
{
const char* t_m = t_c6can(t_t);
if (t_c6_in_i64(t_m)) {
{
return (t_Val){.tag = 2, .iv = t_c6_i64v(t_m), .b = 0, .s = "", .cx = t_VNIL};
}
}
return (t_Val){.tag = 2, .iv = 0, .b = 0, .s = t_m, .cx = t_VNIL};
}
t_Val t_v7(const char* t_t) 
{
const char* t_m = t_c6can(t_t);
if (t_c6_i64ok(t_m)) {
{
return (t_Val){.tag = 3, .iv = t_c6_i64v(t_m), .b = 0, .s = "", .cx = t_VNIL};
}
}
return (t_Val){.tag = 3, .iv = 0, .b = 0, .s = t_m, .cx = t_VNIL};
}
t_Val t_v6i(int64_t t_x) 
{
return (t_Val){.tag = 2, .iv = t_x, .b = 0, .s = "", .cx = t_VNIL};
}
const char* t_v6t(t_Val t_v) 
{
if ((strcmp((const char*)((t_v).s), (const char*)("")) != 0)) {
{
return (t_v).s;
}
}
return t_c6_i64s((t_v).iv);
}
const char* t_v7t(t_Val t_v) 
{
if ((strcmp((const char*)((t_v).s), (const char*)("")) != 0)) {
{
return (t_v).s;
}
}
return t_c6_i64s((t_v).iv);
}
t_Val t_vL() 
{
ctron_list* t_v = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_v), "L");
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_v)};
}
ctron_list* t_v_box(t_Val t_v) 
{
if (((t_v).tag == 8)) {
{
return t_cx_of(t_v);
}
}
if (((t_v).tag == 1)) {
{
ctron_list* t_b1 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b1), "I");
ctron_list_push((ctron_list*)(t_b1), t_dvi_str((int32_t)((t_v).iv)));
return t_b1;
}
}
if (((t_v).tag == 4)) {
{
ctron_list* t_b2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b2), "B");
if ((t_v).b) {
{
ctron_list_push((ctron_list*)(t_b2), "true");
}
}
else {
{
ctron_list_push((ctron_list*)(t_b2), "false");
}
}
return t_b2;
}
}
if (((t_v).tag == 5)) {
{
ctron_list* t_b3 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b3), "S");
ctron_list_push((ctron_list*)(t_b3), (t_v).s);
return t_b3;
}
}
if (((t_v).tag == 2)) {
{
ctron_list* t_b4 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b4), "6");
ctron_list_push((ctron_list*)(t_b4), t_v6t(t_v));
return t_b4;
}
}
if (((t_v).tag == 3)) {
{
ctron_list* t_b5 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b5), "7");
ctron_list_push((ctron_list*)(t_b5), t_v7t(t_v));
return t_b5;
}
}
if (((t_v).tag == 7)) {
{
ctron_list* t_b6 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b6), "W");
ctron_list_push((ctron_list*)(t_b6), t_dvi_str((int32_t)((t_v).iv)));
ctron_list_push((ctron_list*)(t_b6), (t_v).s);
return t_b6;
}
}
if (((t_v).tag == 6)) {
{
ctron_list* t_b7 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b7), "D");
ctron_list_push((ctron_list*)(t_b7), (t_v).s);
return t_b7;
}
}
ctron_list* t_b0 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_b0), "V");
return t_b0;
}
t_Val t_v_unbox(ctron_list* t_l) 
{
const char* t_k0 = ((const char*)((ctron_list*)(t_l))->items[0]);
if ((strcmp((const char*)(t_k0), (const char*)("I")) == 0)) {
{
return t_vI(t_dvi(((const char*)((ctron_list*)(t_l))->items[1])));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("B")) == 0)) {
{
return t_vB(t_seq2(((const char*)((ctron_list*)(t_l))->items[1]), "true"));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("S")) == 0)) {
{
return t_vS(((const char*)((ctron_list*)(t_l))->items[1]));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("V")) == 0)) {
{
return t_vV2();
}
}
if ((strcmp((const char*)(t_k0), (const char*)("6")) == 0)) {
{
return t_v6(((const char*)((ctron_list*)(t_l))->items[1]));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("7")) == 0)) {
{
return t_v7(((const char*)((ctron_list*)(t_l))->items[1]));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("W")) == 0)) {
{
return t_vW(((const char*)((ctron_list*)(t_l))->items[2]), t_dvi(((const char*)((ctron_list*)(t_l))->items[1])));
}
}
if ((strcmp((const char*)(t_k0), (const char*)("D")) == 0)) {
{
return (t_Val){.tag = 6, .iv = 0, .b = 0, .s = ((const char*)((ctron_list*)(t_l))->items[1]), .cx = t_VNIL};
}
}
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_l)};
}
t_Val t_cx_val(ctron_list* t_l) 
{
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_l};
}
const char* t_vtag_str(t_Val t_v) 
{
if (((t_v).tag == 0)) {
{
return "V";
}
}
if (((t_v).tag == 1)) {
{
return "I";
}
}
if (((t_v).tag == 2)) {
{
return "6";
}
}
if (((t_v).tag == 3)) {
{
return "7";
}
}
if (((t_v).tag == 4)) {
{
return "B";
}
}
if (((t_v).tag == 5)) {
{
return "S";
}
}
if (((t_v).tag == 6)) {
{
return "D";
}
}
if (((t_v).tag == 7)) {
{
return "W";
}
}
return ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]);
}
ctron_list* t_vdeep(ctron_list* t_v) 
{
const char* t_k0 = ((const char*)((ctron_list*)(t_v))->items[0]);
if ((strcmp((const char*)(t_k0), (const char*)("M")) == 0)) {
{
return t_v;
}
}
if ((strcmp((const char*)(t_k0), (const char*)("U")) == 0)) {
{
ctron_list* t_ou = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ou), "U");
ctron_list_push((ctron_list*)(t_ou), ((const char*)((ctron_list*)(t_v))->items[1]));
int32_t t_iu = 2;
while (((t_iu + 1) < ((ctron_list*)(t_v))->n)) {
ctron_list_push((ctron_list*)(t_ou), ((const char*)((ctron_list*)(t_v))->items[t_iu]));
ctron_list_push((ctron_list*)(t_ou), (char*)(t_vdeep((ctron_list*)(((const char*)((ctron_list*)(t_v))->items[(t_iu + 1)])))));
t_iu += 2;
}
return t_ou;
}
}
if (t_or2((strcmp((const char*)(t_k0), (const char*)("A")) == 0), (strcmp((const char*)(t_k0), (const char*)("L")) == 0))) {
{
ctron_list* t_oa = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_oa), t_k0);
int32_t t_ia = 1;
while ((t_ia < ((ctron_list*)(t_v))->n)) {
ctron_list_push((ctron_list*)(t_oa), (char*)(t_vdeep((ctron_list*)(((const char*)((ctron_list*)(t_v))->items[t_ia])))));
t_ia += 1;
}
return t_oa;
}
}
ctron_list* t_out = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_v))->items[0]));
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_v))->n)) {
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_v))->items[t_i]));
t_i += 1;
}
return t_out;
}
int32_t t_dvi(const char* t_t) 
{
int t_neg = 0;
int32_t t_k = 0;
if (((strlen((const char*)(t_t)) > 0) && (ctron_byte_at(t_t, 0) == 45))) {
{
t_neg = 1;
t_k = 1;
}
}
if (((t_neg == 0) && ((strlen((const char*)(t_t)) - t_k) <= 10))) {
{
int t_allc = 1;
int32_t t_q = t_k;
{
int32_t t_c = 0;
while ((t_q < strlen((const char*)(t_t)))) {
t_c = ctron_byte_at(t_t, t_q);
if ((t_c < 48)) {
{
t_allc = 0;
}
}
if ((t_c > 57)) {
{
t_allc = 0;
}
}
t_q += 1;
}
}
if (t_allc) {
{
int64_t t_v = 0;
int32_t t_p2 = t_k;
while ((t_p2 < strlen((const char*)(t_t)))) {
t_v = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_v), (int64_t)(10))), (int64_t)((ctron_byte_at(t_t, t_p2) - 48)));
t_p2 += 1;
}
return (int32_t)(t_v);
}
}
}
}
const char* t_acc7 = "0";
while ((t_k < strlen((const char*)(t_t)))) {
t_acc7 = t_c6add(t_c6mul(t_acc7, "10"), t_dvi_str(t_digit_of(ctron_byte_at(t_t, t_k))));
t_k += 1;
}
const char* t_qr = t_c6divmod(t_acc7, "4294967296");
int32_t t_bar = 0;
int32_t t_ci = 0;
while ((t_ci < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_ci) == 124)) {
{
t_bar = t_ci;
}
}
t_ci += 1;
}
const char* t_r = ctron_byte_slice(t_qr, (t_bar + 1), strlen((const char*)(t_qr)));
if (t_neg) {
{
if ((strcmp((const char*)(t_r), (const char*)("0")) != 0)) {
{
t_r = t_c6sub("4294967296", t_r);
}
}
}
}
if ((t_c6abscmp(t_r, "2147483647") > 0)) {
{
t_r = ctron_str_concat((const char*)("-"), (const char*)(t_c6sub("4294967296", t_r)));
}
}
if (t_seq2(t_r, "-2147483648")) {
{
return ((0 - 2147483647) - 1);
}
}
int32_t t_res = 0;
int32_t t_k2 = 0;
int t_neg2 = 0;
if (((strlen((const char*)(t_r)) > 0) && (ctron_byte_at(t_r, 0) == 45))) {
{
t_neg2 = 1;
t_k2 = 1;
}
}
while ((t_k2 < strlen((const char*)(t_r)))) {
t_res = ((t_res * 10) + t_digit_of(ctron_byte_at(t_r, t_k2)));
t_k2 += 1;
}
if (t_neg2) {
{
t_res = (0 - t_res);
}
}
return t_res;
}
int t_truth(t_Val t_v) 
{
if (((t_v).tag == 4)) {
{
return (t_v).b;
}
}
if (((t_v).tag == 1)) {
{
return ((t_v).iv != 0);
}
}
if (((t_v).tag == 6)) {
{
return (strcmp((const char*)((t_v).s), (const char*)("0")) != 0);
}
}
return 1;
}
int t_veq(t_Val t_a, t_Val t_b) 
{
if (t_or2(((t_a).tag == 6), ((t_b).tag == 6))) {
{
return (t_vcmp(t_a, t_b) == 0);
}
}
if ((((t_a).tag == 1) && ((t_b).tag == 1))) {
{
return ((t_a).iv == (t_b).iv);
}
}
if (t_or2(((t_a).tag == 2), ((t_b).tag == 2))) {
{
if ((strcmp((const char*)((t_a).s), (const char*)("")) == 0)) {
{
if ((strcmp((const char*)((t_b).s), (const char*)("")) == 0)) {
{
return ((t_a).iv == (t_b).iv);
}
}
}
}
return (t_c6cmp(t_c6can(t_v6t(t_a)), t_c6can(t_v6t(t_b))) == 0);
}
}
if (t_or2(((t_a).tag == 3), ((t_b).tag == 3))) {
{
return (t_vcmp(t_a, t_b) == 0);
}
}
if (t_or2(((t_a).tag == 7), ((t_b).tag == 7))) {
{
if ((t_or2(((t_a).tag == 1), ((t_a).tag == 7)) && t_or2(((t_b).tag == 1), ((t_b).tag == 7)))) {
{
return ((t_a).iv == (t_b).iv);
}
}
return 0;
}
}
if ((((t_a).tag == 4) && ((t_b).tag == 4))) {
{
return ((t_a).b == (t_b).b);
}
}
if ((((t_a).tag == 5) && ((t_b).tag == 5))) {
{
return t_seq2((t_a).s, (t_b).s);
}
}
if ((((t_a).tag == 8) && ((t_b).tag == 8))) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[0]), "T") && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_b)))->items[0]), "T"))) {
{
if ((!t_seq2(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[1]), ((const char*)((ctron_list*)(t_cx_of(t_b)))->items[1])))) {
{
return 0;
}
}
if ((((ctron_list*)(t_cx_of(t_a)))->n == 3)) {
{
if ((((ctron_list*)(t_cx_of(t_b)))->n != 3)) {
{
return 0;
}
}
return t_veq(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[2]))), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_b)))->items[2]))));
}
}
return (((ctron_list*)(t_cx_of(t_b)))->n == 2);
}
}
}
}
return 0;
}
int32_t t_scmp(const char* t_a, const char* t_b) 
{
int32_t t_n = strlen((const char*)(t_a));
if ((strlen((const char*)(t_b)) < t_n)) {
{
t_n = strlen((const char*)(t_b));
}
}
int32_t t_i = 0;
{
int32_t t_x = 0;
int32_t t_y = 0;
while ((t_i < t_n)) {
t_x = ctron_byte_at(t_a, t_i);
t_y = ctron_byte_at(t_b, t_i);
if ((t_x < t_y)) {
{
return (-1);
}
}
if ((t_x > t_y)) {
{
return 1;
}
}
t_i += 1;
}
}
if ((strlen((const char*)(t_a)) < strlen((const char*)(t_b)))) {
{
return (-1);
}
}
if ((strlen((const char*)(t_a)) > strlen((const char*)(t_b)))) {
{
return 1;
}
}
return 0;
}
int t_c6_i64ok(const char* t_t) 
{
int32_t t_i = 0;
if ((strlen((const char*)(t_t)) > 0)) {
{
if ((ctron_byte_at(t_t, 0) == 45)) {
{
t_i = 1;
}
}
}
}
int32_t t_d = (strlen((const char*)(t_t)) - t_i);
if ((t_d < 1)) {
{
return 0;
}
}
if ((t_d > 18)) {
{
return 0;
}
}
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_t)))) {
t_c = ctron_byte_at(t_t, t_i);
if ((t_c < 48)) {
{
return 0;
}
}
if ((t_c > 57)) {
{
return 0;
}
}
t_i += 1;
}
}
return 1;
}
int64_t t_c6_i64v(const char* t_t) 
{
int t_neg = 0;
int32_t t_i = 0;
if ((strlen((const char*)(t_t)) > 0)) {
{
if ((ctron_byte_at(t_t, 0) == 45)) {
{
t_neg = 1;
t_i = 1;
}
}
}
}
int64_t t_v = 0;
if (t_neg) {
{
while ((t_i < strlen((const char*)(t_t)))) {
t_v = ctron_i64_sub((int64_t)(ctron_i64_mul((int64_t)(t_v), (int64_t)(10))), (int64_t)((ctron_byte_at(t_t, t_i) - 48)));
t_i += 1;
}
return t_v;
}
}
while ((t_i < strlen((const char*)(t_t)))) {
t_v = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_v), (int64_t)(10))), (int64_t)((ctron_byte_at(t_t, t_i) - 48)));
t_i += 1;
}
return t_v;
}
const char* t_c6_i64s(int64_t t_v) 
{
return ctron_i64_to_string(t_v);
}
const char* t_c6can(const char* t_t) 
{
int t_neg = 0;
int32_t t_i = 0;
if ((strlen((const char*)(t_t)) > 0)) {
{
if ((ctron_byte_at(t_t, 0) == 45)) {
{
t_neg = 1;
t_i = 1;
}
}
}
}
int t_strip = 1;
while (t_strip) {
if ((t_i >= (strlen((const char*)(t_t)) - 1))) {
{
t_strip = 0;
}
}
else {
{
if ((ctron_byte_at(t_t, t_i) != 48)) {
{
t_strip = 0;
}
}
else {
{
t_i += 1;
}
}
}
}
}
const char* t_m = ctron_byte_slice(t_t, t_i, strlen((const char*)(t_t)));
if (t_neg) {
{
if ((strcmp((const char*)(t_m), (const char*)("0")) == 0)) {
{
return "0";
}
}
return ctron_str_concat((const char*)("-"), (const char*)(t_m));
}
}
if ((strcmp((const char*)(t_m), (const char*)("")) == 0)) {
{
return "0";
}
}
return t_m;
}
int32_t t_c6abscmp(const char* t_a, const char* t_b) 
{
if ((strlen((const char*)(t_a)) != strlen((const char*)(t_b)))) {
{
if ((strlen((const char*)(t_a)) > strlen((const char*)(t_b)))) {
{
return 1;
}
}
return (-1);
}
}
int32_t t_i = 0;
{
int32_t t_x = 0;
int32_t t_y = 0;
while ((t_i < strlen((const char*)(t_a)))) {
t_x = ctron_byte_at(t_a, t_i);
t_y = ctron_byte_at(t_b, t_i);
if ((t_x > t_y)) {
{
return 1;
}
}
if ((t_x < t_y)) {
{
return (-1);
}
}
t_i += 1;
}
}
return 0;
}
const char* t_lit_digits(const char* t_t) 
{
const char* t_out = "";
int32_t t_i = 0;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_t)))) {
t_c = ctron_byte_at(t_t, t_i);
if ((t_c != 95)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_t, t_i, (t_i + 1))));
}
}
t_i += 1;
}
}
return t_out;
}
int t_c6_in_u64(const char* t_r) 
{
if ((strlen((const char*)(t_r)) > 0)) {
{
if ((ctron_byte_at(t_r, 0) == 45)) {
{
return 0;
}
}
}
}
return (t_c6abscmp(t_r, "18446744073709551615") <= 0);
}
int t_c6_in_i64(const char* t_r) 
{
int t_neg = 0;
const char* t_m = t_r;
if ((strlen((const char*)(t_m)) > 0)) {
{
if ((ctron_byte_at(t_m, 0) == 45)) {
{
t_neg = 1;
t_m = ctron_byte_slice(t_m, 1, strlen((const char*)(t_m)));
}
}
}
}
if (t_neg) {
{
return (t_c6abscmp(t_m, "9223372036854775808") <= 0);
}
}
return (t_c6abscmp(t_m, "9223372036854775807") <= 0);
}
int t_lit_is_dec(const char* t_t) 
{
if ((strlen((const char*)(t_t)) > 1)) {
{
if ((ctron_byte_at(t_t, 0) == 48)) {
{
int32_t t_c2 = ctron_byte_at(t_t, 1);
if (t_or2((t_c2 == 120), t_or2((t_c2 == 111), (t_c2 == 98)))) {
{
return 0;
}
}
}
}
}
}
return 1;
}
const char* t_lit_radix_dec(const char* t_t) 
{
int32_t t_radix = 16;
if ((ctron_byte_at(t_t, 1) == 111)) {
{
t_radix = 8;
}
}
if ((ctron_byte_at(t_t, 1) == 98)) {
{
t_radix = 2;
}
}
const char* t_rs = "16";
if ((t_radix == 8)) {
{
t_rs = "8";
}
}
if ((t_radix == 2)) {
{
t_rs = "2";
}
}
const char* t_acc = "0";
int32_t t_i = 2;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_t)))) {
t_c = ctron_byte_at(t_t, t_i);
if ((t_c != 95)) {
{
int32_t t_dv = t_digit_of(t_c);
t_acc = t_c6add(t_c6mul(t_acc, t_rs), ctron_byte_slice("00010203040506070809101112131415", (t_dv * 2), ((t_dv * 2) + 2)));
}
}
t_i += 1;
}
}
return t_c6can(t_acc);
}
const char* t_lit_to_dec(const char* t_t) 
{
if (t_lit_is_dec(t_t)) {
{
return t_c6can(t_lit_digits(t_t));
}
}
return t_lit_radix_dec(t_t);
}
int t_lit_wide_i32(const char* t_t) 
{
if (t_lit_is_dec(t_t)) {
{
int32_t t_i = 0;
if ((strlen((const char*)(t_t)) > 0)) {
{
if ((ctron_byte_at(t_t, 0) == 45)) {
{
t_i = 1;
}
}
}
}
int32_t t_base = t_i;
{
int32_t t_c0 = 0;
while ((t_base < strlen((const char*)(t_t)))) {
t_c0 = ctron_byte_at(t_t, t_base);
if (((t_c0 != 48) && (t_c0 != 95))) {
{
break;
}
}
t_base += 1;
}
}
int32_t t_eff = 0;
int32_t t_j = t_base;
while ((t_j < strlen((const char*)(t_t)))) {
if ((ctron_byte_at(t_t, t_j) != 95)) {
{
t_eff += 1;
}
}
t_j += 1;
}
if ((t_eff < 10)) {
{
return 0;
}
}
if ((t_eff > 10)) {
{
return 1;
}
}
const char* t_limit = "2147483647";
int32_t t_k = 0;
int32_t t_pos = t_base;
{
int32_t t_c = 0;
int32_t t_l = 0;
while ((t_k < 10)) {
while ((ctron_byte_at(t_t, t_pos) == 95)) {
t_pos += 1;
}
t_c = (ctron_byte_at(t_t, t_pos) - 48);
t_l = (ctron_byte_at(t_limit, t_k) - 48);
if ((t_c < t_l)) {
{
return 0;
}
}
if ((t_c > t_l)) {
{
return 1;
}
}
t_pos += 1;
t_k += 1;
}
}
return 0;
}
}
return (t_c6abscmp(t_lit_to_dec(t_t), "2147483647") > 0);
}
int32_t t_c6cmp(const char* t_a, const char* t_b) 
{
int t_aneg = 0;
int t_bneg = 0;
if ((strlen((const char*)(t_a)) > 0)) {
{
if ((ctron_byte_at(t_a, 0) == 45)) {
{
t_aneg = 1;
}
}
}
}
if ((strlen((const char*)(t_b)) > 0)) {
{
if ((ctron_byte_at(t_b, 0) == 45)) {
{
t_bneg = 1;
}
}
}
}
if (t_aneg) {
{
if (t_bneg) {
{
return t_c6abscmp(t_b, t_a);
}
}
return (-1);
}
}
if (t_bneg) {
{
return 1;
}
}
return t_c6abscmp(t_a, t_b);
}
const char* t_c6add(const char* t_a, const char* t_b) 
{
if ((t_c6_i64ok(t_a) && t_c6_i64ok(t_b))) {
{
return t_c6_i64s(ctron_i64_add((int64_t)(t_c6_i64v(t_a)), (int64_t)(t_c6_i64v(t_b))));
}
}
int t_aneg = 0;
int t_bneg = 0;
if ((strlen((const char*)(t_a)) > 0)) {
{
if ((ctron_byte_at(t_a, 0) == 45)) {
{
t_aneg = 1;
}
}
}
}
if ((strlen((const char*)(t_b)) > 0)) {
{
if ((ctron_byte_at(t_b, 0) == 45)) {
{
t_bneg = 1;
}
}
}
}
const char* t_am = t_c6can(t_a);
const char* t_bm = t_c6can(t_b);
if (t_aneg) {
{
t_am = ctron_byte_slice(t_am, 1, strlen((const char*)(t_am)));
}
}
if (t_bneg) {
{
t_bm = ctron_byte_slice(t_bm, 1, strlen((const char*)(t_bm)));
}
}
if (t_aneg) {
{
if (t_bneg) {
{
return ctron_str_concat((const char*)("-"), (const char*)(t_c6addmag(t_am, t_bm)));
}
}
int32_t t_c = t_c6abscmp(t_am, t_bm);
if ((t_c == 0)) {
{
return "0";
}
}
if ((t_c > 0)) {
{
return ctron_str_concat((const char*)("-"), (const char*)(t_c6submag(t_am, t_bm)));
}
}
return t_c6submag(t_bm, t_am);
}
}
if (t_bneg) {
{
int32_t t_c2 = t_c6abscmp(t_am, t_bm);
if ((t_c2 == 0)) {
{
return "0";
}
}
if ((t_c2 > 0)) {
{
return t_c6submag(t_am, t_bm);
}
}
return ctron_str_concat((const char*)("-"), (const char*)(t_c6submag(t_bm, t_am)));
}
}
return t_c6addmag(t_am, t_bm);
}
const char* t_c6addmag(const char* t_a, const char* t_b) 
{
const char* t_out = "";
int32_t t_ia = strlen((const char*)(t_a));
int32_t t_ib = strlen((const char*)(t_b));
int32_t t_carry = 0;
{
int32_t t_d = 0;
while ((t_ia > 0)) {
t_ia -= 1;
t_d = ((ctron_byte_at(t_a, t_ia) - 48) + t_carry);
if ((t_ib > 0)) {
{
t_ib -= 1;
t_d += (ctron_byte_at(t_b, t_ib) - 48);
}
}
if ((t_d > 9)) {
{
t_d -= 10;
t_carry = 1;
}
}
else {
{
t_carry = 0;
}
}
t_out = ctron_str_concat((const char*)(ctron_byte_slice("0123456789", t_d, (t_d + 1))), (const char*)(t_out));
}
}
{
int32_t t_d2 = 0;
while ((t_ib > 0)) {
t_ib -= 1;
t_d2 = ((ctron_byte_at(t_b, t_ib) - 48) + t_carry);
if ((t_d2 > 9)) {
{
t_d2 -= 10;
t_carry = 1;
}
}
else {
{
t_carry = 0;
}
}
t_out = ctron_str_concat((const char*)(ctron_byte_slice("0123456789", t_d2, (t_d2 + 1))), (const char*)(t_out));
}
}
if ((t_carry == 1)) {
{
t_out = ctron_str_concat((const char*)("1"), (const char*)(t_out));
}
}
return t_c6can(t_out);
}
const char* t_c6submag(const char* t_a, const char* t_b) 
{
const char* t_out = "";
int32_t t_ia = strlen((const char*)(t_a));
int32_t t_ib = strlen((const char*)(t_b));
int32_t t_borrow = 0;
{
int32_t t_d = 0;
while ((t_ia > 0)) {
t_ia -= 1;
t_d = ((ctron_byte_at(t_a, t_ia) - 48) - t_borrow);
if ((t_ib > 0)) {
{
t_ib -= 1;
t_d -= (ctron_byte_at(t_b, t_ib) - 48);
}
}
if ((t_d < 0)) {
{
t_d += 10;
t_borrow = 1;
}
}
else {
{
t_borrow = 0;
}
}
t_out = ctron_str_concat((const char*)(ctron_byte_slice("0123456789", t_d, (t_d + 1))), (const char*)(t_out));
}
}
return t_c6can(t_out);
}
const char* t_c6sub(const char* t_a, const char* t_b) 
{
if ((t_c6_i64ok(t_a) && t_c6_i64ok(t_b))) {
{
return t_c6_i64s(ctron_i64_sub((int64_t)(t_c6_i64v(t_a)), (int64_t)(t_c6_i64v(t_b))));
}
}
int t_aneg = 0;
int t_bneg = 0;
if ((strlen((const char*)(t_a)) > 0)) {
{
if ((ctron_byte_at(t_a, 0) == 45)) {
{
t_aneg = 1;
}
}
}
}
if ((strlen((const char*)(t_b)) > 0)) {
{
if ((ctron_byte_at(t_b, 0) == 45)) {
{
t_bneg = 1;
}
}
}
}
if (t_or2(t_aneg, t_bneg)) {
{
if (t_aneg) {
{
if (t_bneg) {
{
return t_c6sub(ctron_byte_slice(t_a, 1, strlen((const char*)(t_a))), ctron_byte_slice(t_b, 1, strlen((const char*)(t_b))));
}
}
return ctron_str_concat((const char*)("-"), (const char*)(t_c6add(ctron_byte_slice(t_a, 1, strlen((const char*)(t_a))), t_b)));
}
}
return t_c6add(t_a, ctron_byte_slice(t_b, 1, strlen((const char*)(t_b))));
}
}
int32_t t_c = t_c6abscmp(t_a, t_b);
if ((t_c == 0)) {
{
return "0";
}
}
if ((t_c > 0)) {
{
return t_c6submag(t_a, t_b);
}
}
return ctron_str_concat((const char*)("-"), (const char*)(t_c6submag(t_b, t_a)));
}
const char* t_c6mul(const char* t_a, const char* t_b) 
{
if ((t_c6_i64ok(t_a) && t_c6_i64ok(t_b))) {
{
int64_t t_va = t_c6_i64v(t_a);
int64_t t_vb = t_c6_i64v(t_b);
if (((((t_va >= (-2000000000)) && (t_va <= 2000000000)) && (t_vb >= (-2000000000))) && (t_vb <= 2000000000))) {
{
return t_c6_i64s(ctron_i64_mul((int64_t)(t_va), (int64_t)(t_vb)));
}
}
}
}
const char* t_acc = "0";
int32_t t_ib = strlen((const char*)(t_b));
{
int32_t t_d = 0;
int32_t t_zeros = 0;
while ((t_ib > 0)) {
t_ib -= 1;
t_d = (ctron_byte_at(t_b, t_ib) - 48);
const char* t_part = "";
int32_t t_carry = 0;
int32_t t_ia = strlen((const char*)(t_a));
{
int32_t t_p = 0;
while ((t_ia > 0)) {
t_ia -= 1;
t_p = (((ctron_byte_at(t_a, t_ia) - 48) * t_d) + t_carry);
t_carry = (t_p / 10);
t_part = ctron_str_concat((const char*)(ctron_byte_slice("0123456789", (t_p % 10), ((t_p % 10) + 1))), (const char*)(t_part));
}
}
while ((t_carry > 0)) {
t_part = ctron_str_concat((const char*)(ctron_byte_slice("0123456789", (t_carry % 10), ((t_carry % 10) + 1))), (const char*)(t_part));
t_carry = (t_carry / 10);
}
t_zeros = ((strlen((const char*)(t_b)) - 1) - t_ib);
int32_t t_z = 0;
while ((t_z < t_zeros)) {
t_part = ctron_str_concat((const char*)(t_part), (const char*)("0"));
t_z += 1;
}
t_acc = t_c6add(t_acc, t_part);
}
}
return t_c6can(t_acc);
}
const char* t_c6divmod(const char* t_a, const char* t_b) 
{
const char* t_q = "";
const char* t_rem = "0";
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_a)))) {
t_rem = t_c6can(ctron_str_concat((const char*)(t_rem), (const char*)(ctron_byte_slice(t_a, t_i, (t_i + 1)))));
int32_t t_qd = 0;
while ((t_c6abscmp(t_rem, t_b) >= 0)) {
t_rem = t_c6submag(t_rem, t_b);
t_qd += 1;
}
t_q = ctron_str_concat((const char*)(t_q), (const char*)(ctron_byte_slice("0123456789", t_qd, (t_qd + 1))));
t_i += 1;
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_q), (const char*)("|"))), (const char*)(t_rem));
}
int32_t t_vcmp(t_Val t_a, t_Val t_b) 
{
if ((((t_a).tag == 5) && ((t_b).tag == 5))) {
{
return t_scmp((t_a).s, (t_b).s);
}
}
if (t_or2(((t_a).tag == 6), ((t_b).tag == 6))) {
{
return t_df_vcmp((t_a).s, (t_b).s);
}
}
if (t_or2(((t_a).tag == 2), t_or2(((t_b).tag == 2), t_or2(((t_a).tag == 1), ((t_b).tag == 1))))) {
{
if (t_or2(((t_a).tag == 2), ((t_b).tag == 2))) {
{
if ((strcmp((const char*)((t_a).s), (const char*)("")) == 0)) {
{
if ((strcmp((const char*)((t_b).s), (const char*)("")) == 0)) {
{
if (((t_a).iv < (t_b).iv)) {
{
return (-1);
}
}
if (((t_a).iv > (t_b).iv)) {
{
return 1;
}
}
return 0;
}
}
}
}
return t_c6cmp(t_v6t(t_a), t_v6t(t_b));
}
}
}
}
if (t_or2(((t_a).tag == 3), ((t_b).tag == 3))) {
{
const char* t_at7 = t_v7t(t_a);
const char* t_bt7 = t_v7t(t_b);
if (((t_a).tag == 7)) {
{
t_at7 = t_c6_i64s((t_a).iv);
}
}
if (((t_b).tag == 7)) {
{
t_bt7 = t_c6_i64s((t_b).iv);
}
}
return t_c6abscmp(t_at7, t_bt7);
}
}
int64_t t_x = (t_a).iv;
int64_t t_y = (t_b).iv;
if ((t_x < t_y)) {
{
return (-1);
}
}
if ((t_x > t_y)) {
{
return 1;
}
}
return 0;
}
int32_t t_ari(const char* t_op, int32_t t_x, int32_t t_y) 
{
if ((strcmp((const char*)(t_op), (const char*)("Add")) == 0)) {
{
return (t_x + t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("Sub")) == 0)) {
{
return (t_x - t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("Mul")) == 0)) {
{
return (t_x * t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("Div")) == 0)) {
{
return (t_x / t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("Mod")) == 0)) {
{
return (t_x % t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("WrapAdd")) == 0)) {
{
return (t_x + t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("WrapSub")) == 0)) {
{
return (t_x - t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("AddEq")) == 0)) {
{
return (t_x + t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("SubEq")) == 0)) {
{
return (t_x - t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("MulEq")) == 0)) {
{
return (t_x * t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("DivEq")) == 0)) {
{
return (t_x / t_y);
}
}
if ((strcmp((const char*)(t_op), (const char*)("ModEq")) == 0)) {
{
return (t_x % t_y);
}
}
return 0;
}
t_Val t_val_arith(const char* t_op, t_Val t_a, t_Val t_b) 
{
if (t_or2(((t_a).tag == 6), ((t_b).tag == 6))) {
{
return t_vD(t_df_bin(t_op, (t_a).s, (t_b).s));
}
}
if (t_or2(((t_a).tag == 2), ((t_b).tag == 2))) {
{
if ((strcmp((const char*)((t_a).s), (const char*)("")) == 0)) {
{
if ((strcmp((const char*)((t_b).s), (const char*)("")) == 0)) {
{
if (t_or2(t_seq2(t_op, "Sub"), t_seq2(t_op, "SubEq"))) {
{
return t_v6i(ctron_i64_sub((int64_t)((t_a).iv), (int64_t)((t_b).iv)));
}
}
if (t_or2(t_seq2(t_op, "Add"), t_seq2(t_op, "AddEq"))) {
{
return t_v6i(ctron_i64_add((int64_t)((t_a).iv), (int64_t)((t_b).iv)));
}
}
if (t_or2(t_seq2(t_op, "Mul"), t_seq2(t_op, "MulEq"))) {
{
return t_v6i(ctron_i64_mul((int64_t)((t_a).iv), (int64_t)((t_b).iv)));
}
}
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
if (((t_b).iv == 0)) {
{
ctron_panic("division by zero");
}
}
return t_v6i(ctron_i64_div((int64_t)((t_a).iv), (int64_t)((t_b).iv)));
}
}
if (t_or2(t_seq2(t_op, "Mod"), t_seq2(t_op, "ModEq"))) {
{
if (((t_b).iv == 0)) {
{
ctron_panic("division by zero");
}
}
return t_v6i(ctron_i64_mod((int64_t)((t_a).iv), (int64_t)((t_b).iv)));
}
}
ctron_panic("I64 arithmetic: 不支持的算符");
}
}
}
}
const char* t_ta = t_v6t(t_a);
const char* t_tb = t_v6t(t_b);
if (t_or2(t_seq2(t_op, "Sub"), t_seq2(t_op, "SubEq"))) {
{
const char* t_rs = t_c6sub(t_ta, t_tb);
if ((!t_c6_in_i64(t_rs))) {
{
ctron_panic("integer overflow (-)");
}
}
return t_v6(t_rs);
}
}
if (t_or2(t_seq2(t_op, "Add"), t_seq2(t_op, "AddEq"))) {
{
const char* t_ra = t_c6add(t_ta, t_tb);
if ((!t_c6_in_i64(t_ra))) {
{
ctron_panic("integer overflow (+)");
}
}
return t_v6(t_ra);
}
}
if (t_or2(t_seq2(t_op, "Mul"), t_seq2(t_op, "MulEq"))) {
{
const char* t_at = t_ta;
const char* t_bt = t_tb;
int t_aneg = 0;
int t_bneg = 0;
if ((strlen((const char*)(t_at)) > 0)) {
{
if ((ctron_byte_at(t_at, 0) == 45)) {
{
t_aneg = 1;
t_at = ctron_byte_slice(t_at, 1, strlen((const char*)(t_at)));
}
}
}
}
if ((strlen((const char*)(t_bt)) > 0)) {
{
if ((ctron_byte_at(t_bt, 0) == 45)) {
{
t_bneg = 1;
t_bt = ctron_byte_slice(t_bt, 1, strlen((const char*)(t_bt)));
}
}
}
}
const char* t_mag = t_c6mul(t_c6can(t_at), t_c6can(t_bt));
if ((strcmp((const char*)(t_mag), (const char*)("0")) == 0)) {
{
return t_v6("0");
}
}
int t_negr = 0;
if (t_aneg) {
{
if ((!t_bneg)) {
{
t_negr = 1;
}
}
}
}
else {
{
if (t_bneg) {
{
t_negr = 1;
}
}
}
}
if (t_negr) {
{
if ((t_c6abscmp(t_mag, "9223372036854775808") > 0)) {
{
ctron_panic("integer overflow (*)");
}
}
return t_v6(ctron_str_concat((const char*)("-"), (const char*)(t_mag)));
}
}
if ((t_c6abscmp(t_mag, "9223372036854775807") > 0)) {
{
ctron_panic("integer overflow (*)");
}
}
return t_v6(t_mag);
}
}
if (t_or2(t_seq2(t_op, "Div"), t_or2(t_seq2(t_op, "Mod"), t_or2(t_seq2(t_op, "DivEq"), t_seq2(t_op, "ModEq"))))) {
{
if ((strcmp((const char*)(t_tb), (const char*)("0")) == 0)) {
{
ctron_panic("division by zero");
}
}
if ((strcmp((const char*)(t_tb), (const char*)("-1")) == 0)) {
{
if ((strcmp((const char*)(t_ta), (const char*)("-9223372036854775808")) == 0)) {
{
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
ctron_panic("integer overflow (/)");
}
}
ctron_panic("integer overflow (%)");
}
}
}
}
int t_aneg2 = 0;
int t_bneg2 = 0;
if ((strlen((const char*)(t_ta)) > 0)) {
{
if ((ctron_byte_at(t_ta, 0) == 45)) {
{
t_aneg2 = 1;
}
}
}
}
if ((strlen((const char*)(t_tb)) > 0)) {
{
if ((ctron_byte_at(t_tb, 0) == 45)) {
{
t_bneg2 = 1;
}
}
}
}
const char* t_at2 = t_ta;
const char* t_bt2 = t_tb;
if (t_aneg2) {
{
t_at2 = ctron_byte_slice(t_at2, 1, strlen((const char*)(t_at2)));
}
}
if (t_bneg2) {
{
t_bt2 = ctron_byte_slice(t_bt2, 1, strlen((const char*)(t_bt2)));
}
}
const char* t_qr = t_c6divmod(t_at2, t_bt2);
int32_t t_bar = 0;
int32_t t_ci = 0;
while ((t_ci < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_ci) == 124)) {
{
t_bar = t_ci;
}
}
t_ci += 1;
}
const char* t_q = t_c6can(ctron_byte_slice(t_qr, 0, t_bar));
const char* t_r = t_c6can(ctron_byte_slice(t_qr, (t_bar + 1), strlen((const char*)(t_qr))));
if (t_aneg2) {
{
if (t_bneg2) {
{
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
return t_v6(t_q);
}
}
return t_v6(ctron_str_concat((const char*)("-"), (const char*)(t_r)));
}
}
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
return t_v6(ctron_str_concat((const char*)("-"), (const char*)(t_q)));
}
}
return t_v6(ctron_str_concat((const char*)("-"), (const char*)(t_r)));
}
}
if (t_bneg2) {
{
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
return t_v6(ctron_str_concat((const char*)("-"), (const char*)(t_q)));
}
}
return t_v6(t_r);
}
}
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
return t_v6(t_q);
}
}
return t_v6(t_r);
}
}
ctron_panic("I64 arithmetic: 不支持的算符");
}
}
if (t_or2(((t_a).tag == 3), ((t_b).tag == 3))) {
{
const char* t_ua = t_v7t(t_a);
const char* t_ub = t_v7t(t_b);
if (t_or2(t_seq2(t_op, "Sub"), t_seq2(t_op, "SubEq"))) {
{
if ((t_c6abscmp(t_ua, t_ub) < 0)) {
{
ctron_panic("integer overflow (-)");
}
}
return t_v7(t_c6sub(t_ua, t_ub));
}
}
if (t_or2(t_seq2(t_op, "Add"), t_seq2(t_op, "AddEq"))) {
{
const char* t_r7a = t_c6add(t_ua, t_ub);
if ((!t_c6_in_u64(t_r7a))) {
{
ctron_panic("integer overflow (+)");
}
}
return t_v7(t_r7a);
}
}
if (t_or2(t_seq2(t_op, "Mul"), t_seq2(t_op, "MulEq"))) {
{
const char* t_m7 = t_c6mul(t_ua, t_ub);
if ((!t_c6_in_u64(t_m7))) {
{
ctron_panic("integer overflow (*)");
}
}
return t_v7(t_m7);
}
}
if (t_or2(t_seq2(t_op, "Div"), t_or2(t_seq2(t_op, "Mod"), t_or2(t_seq2(t_op, "DivEq"), t_seq2(t_op, "ModEq"))))) {
{
if ((strcmp((const char*)(t_ub), (const char*)("0")) == 0)) {
{
ctron_panic("division by zero");
}
}
const char* t_qr7 = t_c6divmod(t_ua, t_ub);
int32_t t_bar7 = 0;
int32_t t_ci7 = 0;
while ((t_ci7 < strlen((const char*)(t_qr7)))) {
if ((ctron_byte_at(t_qr7, t_ci7) == 124)) {
{
t_bar7 = t_ci7;
}
}
t_ci7 += 1;
}
if (t_or2(t_seq2(t_op, "Div"), t_seq2(t_op, "DivEq"))) {
{
return t_v7(ctron_byte_slice(t_qr7, 0, t_bar7));
}
}
return t_v7(ctron_byte_slice(t_qr7, (t_bar7 + 1), strlen((const char*)(t_qr7))));
}
}
ctron_panic("U64 算术: 不支持的算符");
}
}
if (t_or2(t_seq2(t_op, "Div"), t_or2(t_seq2(t_op, "Mod"), t_or2(t_seq2(t_op, "DivEq"), t_seq2(t_op, "ModEq"))))) {
{
if (((t_b).iv == 0)) {
{
ctron_panic("division by zero");
}
}
}
}
if (t_or2(((t_a).tag == 7), ((t_b).tag == 7))) {
{
return t_w_arith(t_op, t_a, t_b);
}
}
if ((((t_a).tag == 8) && ((t_b).tag == 8))) {
{
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[0])), (const char*)("VEC")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_b)))->items[0])), (const char*)("VEC")) == 0))) {
{
ctron_list* t_vr = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_vr), "VEC");
int32_t t_vi2 = 1;
while ((t_vi2 < ((ctron_list*)(t_cx_of(t_a)))->n)) {
ctron_list_push((ctron_list*)(t_vr), (char*)(t_v_box(t_vD(t_df_bin(t_op, (t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[t_vi2])))).s, (t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_b)))->items[t_vi2])))).s)))));
t_vi2 += 1;
}
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_vr)};
}
}
}
}
int32_t t_xa = (int32_t)((t_a).iv);
int32_t t_yb = (int32_t)((t_b).iv);
if (t_or2(t_seq2(t_op, "Add"), t_seq2(t_op, "AddEq"))) {
{
if (t_iadd_ov(t_xa, t_yb)) {
{
ctron_panic("integer overflow (+)");
}
}
}
}
if (t_or2(t_seq2(t_op, "Sub"), t_seq2(t_op, "SubEq"))) {
{
if (t_isub_ov(t_xa, t_yb)) {
{
ctron_panic("integer overflow (-)");
}
}
}
}
if (t_or2(t_seq2(t_op, "Mul"), t_seq2(t_op, "MulEq"))) {
{
if (t_imul_ov(t_xa, t_yb, 2147483647, ((0 - 2147483647) - 1))) {
{
ctron_panic("integer overflow (*)");
}
}
}
}
return t_vI(t_ari(t_op, t_xa, t_yb));
}
t_Val t_vW(const char* t_w, int32_t t_x) 
{
return (t_Val){.tag = 7, .iv = (int64_t)(t_x), .b = 0, .s = t_w};
}
const char* t_w_of(t_Val t_v) 
{
if (((t_v).tag == 7)) {
{
return (t_v).s;
}
}
return "i32";
}
int32_t t_w_rank(const char* t_w) 
{
if ((strcmp((const char*)(t_w), (const char*)("i32")) == 0)) {
{
return 3;
}
}
if (t_or2((strcmp((const char*)(t_w), (const char*)("u16")) == 0), (strcmp((const char*)(t_w), (const char*)("i16")) == 0))) {
{
return 2;
}
}
return 1;
}
int t_w_signed(const char* t_w) 
{
return t_or2((strcmp((const char*)(t_w), (const char*)("i8")) == 0), (strcmp((const char*)(t_w), (const char*)("i16")) == 0));
}
int32_t t_w_max(const char* t_w) 
{
if ((strcmp((const char*)(t_w), (const char*)("u8")) == 0)) {
{
return 255;
}
}
if ((strcmp((const char*)(t_w), (const char*)("i8")) == 0)) {
{
return 127;
}
}
if ((strcmp((const char*)(t_w), (const char*)("u16")) == 0)) {
{
return 65535;
}
}
if ((strcmp((const char*)(t_w), (const char*)("i16")) == 0)) {
{
return 32767;
}
}
return 2147483647;
}
int32_t t_w_min(const char* t_w) 
{
if (t_or2((strcmp((const char*)(t_w), (const char*)("u8")) == 0), (strcmp((const char*)(t_w), (const char*)("u16")) == 0))) {
{
return 0;
}
}
if ((strcmp((const char*)(t_w), (const char*)("i8")) == 0)) {
{
return (0 - 128);
}
}
if ((strcmp((const char*)(t_w), (const char*)("i16")) == 0)) {
{
return (0 - 32768);
}
}
return ((0 - 2147483647) - 1);
}
t_Val t_w_arith(const char* t_op, t_Val t_a, t_Val t_b) 
{
const char* t_wa = t_w_of(t_a);
const char* t_wb = t_w_of(t_b);
const char* t_w = t_wa;
if ((t_w_rank(t_wb) > t_w_rank(t_wa))) {
{
t_w = t_wb;
}
}
int32_t t_x = (int32_t)((t_a).iv);
int32_t t_y = (int32_t)((t_b).iv);
int32_t t_mx = t_w_max(t_w);
int32_t t_mn = t_w_min(t_w);
if (t_or2(t_seq2(t_op, "WrapAdd"), t_seq2(t_op, "WrapSub"))) {
{
int32_t t_t = (t_x + t_y);
if (t_seq2(t_op, "WrapSub")) {
{
t_t = (t_x - t_y);
}
}
int32_t t_s = 1;
int32_t t_k = 0;
while ((t_k < t_w_bits(t_w))) {
t_s = (t_s * 2);
t_k += 1;
}
int32_t t_m = (t_t % t_s);
if ((t_m < 0)) {
{
t_m += t_s;
}
}
if ((t_w_signed(t_w) && (t_m > t_mx))) {
{
t_m -= t_s;
}
}
return t_vW(t_w, t_m);
}
}
if (t_or2(t_seq2(t_op, "Add"), t_seq2(t_op, "AddEq"))) {
{
int32_t t_r = (t_x + t_y);
if (t_or2((t_r > t_mx), (t_r < t_mn))) {
{
ctron_panic("integer overflow (+)");
}
}
return t_vW(t_w, t_r);
}
}
if (t_or2(t_seq2(t_op, "Sub"), t_seq2(t_op, "SubEq"))) {
{
int32_t t_r = (t_x - t_y);
if (t_or2((t_r > t_mx), (t_r < t_mn))) {
{
ctron_panic("integer overflow (-)");
}
}
return t_vW(t_w, t_r);
}
}
if (t_or2(t_seq2(t_op, "Mul"), t_seq2(t_op, "MulEq"))) {
{
if (t_imul_ov(t_x, t_y, t_mx, t_mn)) {
{
ctron_panic("integer overflow (*)");
}
}
return t_vW(t_w, (t_x * t_y));
}
}
return t_vW(t_w, t_ari(t_op, t_x, t_y));
}
int32_t t_w_bits(const char* t_w) 
{
if (t_or2((strcmp((const char*)(t_w), (const char*)("u8")) == 0), (strcmp((const char*)(t_w), (const char*)("i8")) == 0))) {
{
return 8;
}
}
if (t_or2((strcmp((const char*)(t_w), (const char*)("u16")) == 0), (strcmp((const char*)(t_w), (const char*)("i16")) == 0))) {
{
return 16;
}
}
return 32;
}
int t_iadd_ov(int32_t t_x, int32_t t_y) 
{
if ((t_y > 0)) {
{
return (t_x > (2147483647 - t_y));
}
}
if ((t_y < 0)) {
{
return (t_x < (((0 - 2147483647) - 1) - t_y));
}
}
return 0;
}
int t_isub_ov(int32_t t_x, int32_t t_y) 
{
if ((t_y < 0)) {
{
return (t_x > (2147483647 + t_y));
}
}
if ((t_y > 0)) {
{
return (t_x < (((0 - 2147483647) - 1) + t_y));
}
}
return 0;
}
int t_imul_ov(int32_t t_x, int32_t t_y, int32_t t_mx, int32_t t_mn) 
{
if (t_or2((t_x == 0), (t_y == 0))) {
{
return 0;
}
}
if (((t_x > 0) && (t_y > 0))) {
{
return (t_x > (t_mx / t_y));
}
}
if (((t_x > 0) && (t_y < 0))) {
{
return (t_y < (t_mn / t_x));
}
}
if (((t_x < 0) && (t_y > 0))) {
{
return (t_x < (t_mn / t_y));
}
}
return (t_x < (t_mx / t_y));
}
t_Val t_vD(const char* t_t) 
{
return (t_Val){.tag = 6, .iv = 0, .b = 0, .s = t_t};
}
int64_t t_df_p10(int32_t t_k) 
{
int64_t t_r = 1;
int32_t t_i = 0;
while ((t_i < t_k)) {
t_r = ctron_i64_mul((int64_t)(t_r), (int64_t)(10));
t_i += 1;
}
return t_r;
}
int32_t t_df_digits(int64_t t_a) 
{
int32_t t_n = 0;
int64_t t_x = t_a;
if ((t_x < 0)) {
{
t_x = ctron_i64_sub((int64_t)(0), (int64_t)(t_x));
}
}
if ((t_x == 0)) {
{
return 1;
}
}
while ((t_x > 0)) {
t_x = ctron_i64_div((int64_t)(t_x), (int64_t)(10));
t_n += 1;
}
return t_n;
}
const char* t_df_pack(int64_t t_m, int32_t t_s) 
{
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_i64_to_string(t_m)), (const char*)("@"))), (const char*)(ctron_i32_to_string((int32_t)(t_s))));
}
int64_t t_df_pm(const char* t_p) 
{
int64_t t_m = 0;
int32_t t_i = 0;
int t_neg = 0;
if ((ctron_byte_at(t_p, 0) == 45)) {
{
t_neg = 1;
t_i = 1;
}
}
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_p)))) {
t_c = ctron_byte_at(t_p, t_i);
if ((t_c == 64)) {
{
if (t_neg) {
{
t_m = ctron_i64_sub((int64_t)(0), (int64_t)(t_m));
}
}
return t_m;
}
}
t_m = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_m), (int64_t)(10))), (int64_t)((t_c - 48)));
t_i += 1;
}
}
if (t_neg) {
{
t_m = ctron_i64_sub((int64_t)(0), (int64_t)(t_m));
}
}
return t_m;
}
int32_t t_df_ps(const char* t_p) 
{
int32_t t_v = 0;
int32_t t_i = 0;
int t_seen = 0;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_p)))) {
t_c = ctron_byte_at(t_p, t_i);
if ((t_c == 64)) {
{
t_seen = 1;
}
}
else {
{
if (t_seen) {
{
t_v = ((t_v * 10) + (t_c - 48));
}
}
}
}
t_i += 1;
}
}
return t_v;
}
const char* t_df_split(const char* t_t) 
{
int64_t t_sgn = 1;
const char* t_body = t_t;
if (((strlen((const char*)(t_body)) > 0) && (ctron_byte_at(t_body, 0) == 45))) {
{
t_sgn = (0 - 1);
t_body = ctron_byte_slice(t_body, 1, strlen((const char*)(t_body)));
}
}
const char* t_ip = "";
const char* t_fp = "";
int t_dot = 0;
int32_t t_i = 0;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_body)))) {
t_c = ctron_byte_at(t_body, t_i);
if ((t_c == 46)) {
{
t_dot = 1;
}
}
else {
{
if ((!t_dot)) {
{
t_ip = ctron_str_concat((const char*)(t_ip), (const char*)(ctron_byte_slice(t_body, t_i, (t_i + 1))));
}
}
else {
{
if ((strlen((const char*)(t_fp)) < 15)) {
{
t_fp = ctron_str_concat((const char*)(t_fp), (const char*)(ctron_byte_slice(t_body, t_i, (t_i + 1))));
}
}
}
}
}
}
t_i += 1;
}
}
if ((strcmp((const char*)(t_ip), (const char*)("")) == 0)) {
{
t_ip = "0";
}
}
int64_t t_m = 0;
int32_t t_j = 0;
while ((t_j < strlen((const char*)(t_ip)))) {
t_m = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_m), (int64_t)(10))), (int64_t)((ctron_byte_at(t_ip, t_j) - 48)));
t_j += 1;
}
t_j = 0;
while ((t_j < strlen((const char*)(t_fp)))) {
t_m = ctron_i64_add((int64_t)(ctron_i64_mul((int64_t)(t_m), (int64_t)(10))), (int64_t)((ctron_byte_at(t_fp, t_j) - 48)));
t_j += 1;
}
t_m = ctron_i64_mul((int64_t)(t_m), (int64_t)(t_sgn));
return t_df_pack(t_m, strlen((const char*)(t_fp)));
}
int64_t t_df_al(int64_t t_m, int32_t t_s, int32_t t_t) 
{
int64_t t_r = t_m;
int32_t t_i = t_s;
while ((t_i < t_t)) {
t_r = ctron_i64_mul((int64_t)(t_r), (int64_t)(10));
t_i += 1;
}
return t_r;
}
const char* t_df_text(int64_t t_m, int32_t t_s) 
{
int t_neg = (t_m < 0);
int64_t t_a = t_m;
if (t_neg) {
{
t_a = ctron_i64_sub((int64_t)(0), (int64_t)(t_a));
}
}
int32_t t_sc = t_s;
while ((t_sc > 9)) {
t_a = ctron_i64_div((int64_t)(t_a), (int64_t)(10));
t_sc -= 1;
}
while ((((t_a > 0) && (t_sc > 0)) && (ctron_i64_mod((int64_t)(t_a), (int64_t)(10)) == 0))) {
t_a = ctron_i64_div((int64_t)(t_a), (int64_t)(10));
t_sc -= 1;
}
while (((t_df_digits(t_a) > 9) && (t_sc > 0))) {
t_a = ctron_i64_div((int64_t)(t_a), (int64_t)(10));
t_sc -= 1;
}
if ((t_a == 0)) {
{
return "0";
}
}
const char* t_ds = ctron_i64_to_string(t_a);
const char* t_t = "";
if (t_neg) {
{
t_t = "-";
}
}
if ((t_sc == 0)) {
{
return ctron_str_concat((const char*)(t_t), (const char*)(t_ds));
}
}
if ((strlen((const char*)(t_ds)) <= t_sc)) {
{
const char* t_pad = "0.";
int32_t t_z = 0;
while ((t_z < (t_sc - strlen((const char*)(t_ds))))) {
t_pad = ctron_str_concat((const char*)(t_pad), (const char*)("0"));
t_z += 1;
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_t), (const char*)(t_pad))), (const char*)(t_ds));
}
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_t), (const char*)(ctron_byte_slice(t_ds, 0, (strlen((const char*)(t_ds)) - t_sc))))), (const char*)("."))), (const char*)(ctron_byte_slice(t_ds, (strlen((const char*)(t_ds)) - t_sc), strlen((const char*)(t_ds)))));
}
const char* t_df_can(const char* t_t) 
{
const char* t_p = t_df_split(t_t);
return t_df_text(t_df_pm(t_p), t_df_ps(t_p));
}
const char* t_df_bin(const char* t_op, const char* t_ta, const char* t_tb) 
{
const char* t_pa = t_df_split(t_ta);
const char* t_pb = t_df_split(t_tb);
int64_t t_m1 = t_df_pm(t_pa);
int32_t t_s1 = t_df_ps(t_pa);
int64_t t_m2 = t_df_pm(t_pb);
int32_t t_s2 = t_df_ps(t_pb);
if (t_or2((strcmp((const char*)(t_op), (const char*)("Add")) == 0), (strcmp((const char*)(t_op), (const char*)("Sub")) == 0))) {
{
int32_t t_s = t_s1;
if ((t_s2 > t_s)) {
{
t_s = t_s2;
}
}
int64_t t_x = t_df_al(t_m1, t_s1, t_s);
int64_t t_y = t_df_al(t_m2, t_s2, t_s);
if ((strcmp((const char*)(t_op), (const char*)("Sub")) == 0)) {
{
return t_df_text(ctron_i64_sub((int64_t)(t_x), (int64_t)(t_y)), t_s);
}
}
return t_df_text(ctron_i64_add((int64_t)(t_x), (int64_t)(t_y)), t_s);
}
}
if ((strcmp((const char*)(t_op), (const char*)("Mul")) == 0)) {
{
return t_df_text(ctron_i64_mul((int64_t)(t_m1), (int64_t)(t_m2)), (t_s1 + t_s2));
}
}
if ((strcmp((const char*)(t_op), (const char*)("Div")) == 0)) {
{
if ((t_m2 == 0)) {
{
ctron_panic("float div by zero");
return "0";
}
}
int32_t t_k = (17 - t_df_digits(t_m1));
if ((t_k < 0)) {
{
t_k = 0;
}
}
int64_t t_num = ctron_i64_mul((int64_t)(t_m1), (int64_t)(t_df_p10(t_k)));
int64_t t_q = ctron_i64_div((int64_t)(t_num), (int64_t)(t_m2));
int64_t t_rm = ctron_i64_sub((int64_t)(t_num), (int64_t)(ctron_i64_mul((int64_t)(t_q), (int64_t)(t_m2))));
if ((t_rm < 0)) {
{
t_rm = ctron_i64_sub((int64_t)(0), (int64_t)(t_rm));
}
}
int64_t t_m2a = t_m2;
if ((t_m2a < 0)) {
{
t_m2a = ctron_i64_sub((int64_t)(0), (int64_t)(t_m2a));
}
}
int64_t t_qinc = 1;
if ((t_q < 0)) {
{
t_qinc = (0 - 1);
}
}
if (((t_m2a != 0) && (ctron_i64_mul((int64_t)(t_rm), (int64_t)(2)) >= t_m2a))) {
{
t_q = ctron_i64_add((int64_t)(t_q), (int64_t)(t_qinc));
}
}
return t_df_text(t_q, ((t_s1 - t_s2) + t_k));
}
}
ctron_panic(ctron_str_concat((const char*)("df_bin:"), (const char*)(t_op)));
return "0";
}
int32_t t_df_vcmp(const char* t_ta, const char* t_tb) 
{
const char* t_pa = t_df_split(t_ta);
const char* t_pb = t_df_split(t_tb);
int64_t t_m1 = t_df_pm(t_pa);
int32_t t_s1 = t_df_ps(t_pa);
int64_t t_m2 = t_df_pm(t_pb);
int32_t t_s2 = t_df_ps(t_pb);
int32_t t_s = t_s1;
if ((t_s2 > t_s)) {
{
t_s = t_s2;
}
}
int64_t t_x = t_df_al(t_m1, t_s1, t_s);
int64_t t_y = t_df_al(t_m2, t_s2, t_s);
if ((t_x < t_y)) {
{
return (-1);
}
}
if ((t_x > t_y)) {
{
return 1;
}
}
return 0;
}
const char* t_df_add1(const char* t_ds) 
{
const char* t_digits = "0123456789";
int32_t t_i = (strlen((const char*)(t_ds)) - 1);
{
int32_t t_c = 0;
while ((t_i >= 0)) {
t_c = ctron_byte_at(t_ds, t_i);
if ((t_c == 57)) {
{
t_i -= 1;
}
}
else {
{
const char* t_head = ctron_byte_slice(t_ds, 0, t_i);
const char* t_mid = ctron_byte_slice(t_digits, (t_c - 47), (t_c - 46));
const char* t_tail = "";
int32_t t_z = 0;
while ((t_z < ((strlen((const char*)(t_ds)) - t_i) - 1))) {
t_tail = ctron_str_concat((const char*)(t_tail), (const char*)("0"));
t_z += 1;
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_head), (const char*)(t_mid))), (const char*)(t_tail));
}
}
}
}
const char* t_out = "1";
int32_t t_z2 = 0;
while ((t_z2 < strlen((const char*)(t_ds)))) {
t_out = ctron_str_concat((const char*)(t_out), (const char*)("0"));
t_z2 += 1;
}
return t_out;
}
const char* t_df_g(const char* t_t) 
{
int t_neg = (ctron_byte_at(t_t, 0) == 45);
const char* t_body = t_t;
if (t_neg) {
{
t_body = ctron_byte_slice(t_t, 1, strlen((const char*)(t_t)));
}
}
const char* t_ip = "";
const char* t_fp = "";
int t_dot = 0;
int32_t t_i = 0;
{
int32_t t_c = 0;
while ((t_i < strlen((const char*)(t_body)))) {
t_c = ctron_byte_at(t_body, t_i);
if ((t_c == 46)) {
{
t_dot = 1;
}
}
else {
{
if ((!t_dot)) {
{
t_ip = ctron_str_concat((const char*)(t_ip), (const char*)(ctron_byte_slice(t_body, t_i, (t_i + 1))));
}
}
else {
{
t_fp = ctron_str_concat((const char*)(t_fp), (const char*)(ctron_byte_slice(t_body, t_i, (t_i + 1))));
}
}
}
}
t_i += 1;
}
}
const char* t_digits = ctron_str_concat((const char*)(t_ip), (const char*)(t_fp));
while (((strlen((const char*)(t_digits)) > 1) && (ctron_byte_at(t_digits, 0) == 48))) {
t_digits = ctron_byte_slice(t_digits, 1, strlen((const char*)(t_digits)));
}
int32_t t_xe = 0;
if ((strcmp((const char*)(t_ip), (const char*)("0")) == 0)) {
{
int32_t t_z = 0;
while (((t_z < strlen((const char*)(t_fp))) && (ctron_byte_at(t_fp, t_z) == 48))) {
t_z += 1;
}
t_xe = ((0 - t_z) - 1);
}
}
else {
{
t_xe = (strlen((const char*)(t_ip)) - 1);
}
}
const char* t_keep = t_digits;
if ((strlen((const char*)(t_keep)) > 6)) {
{
int32_t t_rest = ctron_byte_at(t_keep, 6);
t_keep = ctron_byte_slice(t_keep, 0, 6);
if ((t_rest >= 53)) {
{
t_keep = t_df_add1(t_keep);
}
}
}
}
while (((strlen((const char*)(t_keep)) > 1) && (ctron_byte_at(t_keep, (strlen((const char*)(t_keep)) - 1)) == 48))) {
t_keep = ctron_byte_slice(t_keep, 0, (strlen((const char*)(t_keep)) - 1));
}
int32_t t_e = t_xe;
if ((strlen((const char*)(t_keep)) > 6)) {
{
t_e = (t_xe + 1);
t_keep = ctron_str_concat((const char*)(ctron_byte_slice(t_keep, 0, 1)), (const char*)(ctron_byte_slice(t_keep, 1, 6)));
}
}
const char* t_sgn = "";
if (t_neg) {
{
t_sgn = "-";
}
}
if (t_or2((t_e < (-4)), (t_e >= 6))) {
{
const char* t_mant = ctron_byte_slice(t_keep, 0, 1);
if ((strlen((const char*)(t_keep)) > 1)) {
{
t_mant = ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_mant), (const char*)("."))), (const char*)(ctron_byte_slice(t_keep, 1, strlen((const char*)(t_keep)))));
}
}
int32_t t_ae = t_e;
const char* t_es = "+";
if ((t_ae < 0)) {
{
t_es = "-";
t_ae = (0 - t_ae);
}
}
const char* t_es2 = ctron_i32_to_string((int32_t)(t_ae));
while ((strlen((const char*)(t_es2)) < 2)) {
t_es2 = ctron_str_concat((const char*)("0"), (const char*)(t_es2));
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_sgn), (const char*)(t_mant))), (const char*)("e"))), (const char*)(t_es))), (const char*)(t_es2));
}
}
if ((t_e >= 0)) {
{
int32_t t_pnt = (t_e + 1);
if ((strlen((const char*)(t_keep)) <= t_pnt)) {
{
const char* t_r = t_keep;
int32_t t_z2 = strlen((const char*)(t_keep));
while ((t_z2 < t_pnt)) {
t_r = ctron_str_concat((const char*)(t_r), (const char*)("0"));
t_z2 += 1;
}
return ctron_str_concat((const char*)(t_sgn), (const char*)(t_r));
}
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_sgn), (const char*)(ctron_byte_slice(t_keep, 0, t_pnt)))), (const char*)("."))), (const char*)(ctron_byte_slice(t_keep, t_pnt, strlen((const char*)(t_keep)))));
}
}
const char* t_pad = "0.";
int32_t t_z3 = 0;
while ((t_z3 < ((0 - t_e) - 1))) {
t_pad = ctron_str_concat((const char*)(t_pad), (const char*)("0"));
t_z3 += 1;
}
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_sgn), (const char*)(t_pad))), (const char*)(t_keep));
}
const char* t_df_show(const char* t_t) 
{
int t_dot = 0;
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_t)))) {
if ((ctron_byte_at(t_t, t_i) == 46)) {
{
t_dot = 1;
}
}
t_i += 1;
}
if ((!t_dot)) {
{
return ctron_str_concat((const char*)(t_t), (const char*)(".0"));
}
}
return t_df_g(t_t);
}
const char* t_fmt(t_Val t_v) 
{
if (((t_v).tag == 1)) {
{
return t_dvi_str((int32_t)((t_v).iv));
}
}
if (((t_v).tag == 7)) {
{
return t_dvi_str((int32_t)((t_v).iv));
}
}
if (((t_v).tag == 2)) {
{
return t_v6t(t_v);
}
}
if (((t_v).tag == 3)) {
{
return t_v7t(t_v);
}
}
if (((t_v).tag == 4)) {
{
if ((t_v).b) {
{
return "true";
}
}
return "false";
}
}
if (((t_v).tag == 5)) {
{
return (t_v).s;
}
}
if (((t_v).tag == 6)) {
{
return t_df_show((t_v).s);
}
}
if (((t_v).tag == 0)) {
{
return "";
}
}
const char* t_k = ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]);
if ((strcmp((const char*)(t_k), (const char*)("T")) == 0)) {
{
if ((((ctron_list*)(t_cx_of(t_v)))->n == 3)) {
{
return ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1])), (const char*)("("))), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[2]))))))), (const char*)(")"));
}
}
return ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]);
}
}
if ((strcmp((const char*)(t_k), (const char*)("U")) == 0)) {
{
return t_fmt_struct(t_cx_of(t_v));
}
}
return "<value>";
}
const char* t_fmt_struct(ctron_list* t_v) 
{
const char* t_s = ctron_str_concat((const char*)(((const char*)((ctron_list*)(t_v))->items[1])), (const char*)("("));
int32_t t_i = 2;
int t_first = 1;
while ((t_i < ((ctron_list*)(t_v))->n)) {
if (t_first) {
{
t_first = 0;
}
}
else {
{
t_s = ctron_str_concat((const char*)(t_s), (const char*)(","));
}
}
t_s = ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_s), (const char*)(((const char*)((ctron_list*)(t_v))->items[t_i])))), (const char*)("="))), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_v))->items[(t_i + 1)]))))));
t_i += 2;
}
return ctron_str_concat((const char*)(t_s), (const char*)(")"));
}
int t_eq_val(t_Val t_a, t_Val t_b) 
{
if (t_or2(((t_a).tag == 3), ((t_b).tag == 3))) {
{
return (t_vcmp(t_a, t_b) == 0);
}
}
if (((t_a).tag != (t_b).tag)) {
{
return 0;
}
}
if (((t_a).tag == 5)) {
{
return t_seq2((t_a).s, (t_b).s);
}
}
if (((t_a).tag == 1)) {
{
return ((t_a).iv == (t_b).iv);
}
}
if (((t_a).tag == 7)) {
{
return ((t_a).iv == (t_b).iv);
}
}
if (((t_a).tag == 4)) {
{
return ((t_a).b == (t_b).b);
}
}
if (((t_a).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[0]), "U")) {
{
if ((!t_seq2(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[1]), ((const char*)((ctron_list*)(t_cx_of(t_b)))->items[1])))) {
{
return 0;
}
}
int32_t t_i = 2;
while ((t_i < ((ctron_list*)(t_cx_of(t_a)))->n)) {
if ((!t_eq_val(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[(t_i + 1)]))), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_b)))->items[(t_i + 1)])))))) {
{
return 0;
}
}
t_i += 2;
}
return 1;
}
}
}
}
ctron_panic(ctron_str_concat((const char*)("eq 字段型别:"), (const char*)(((const char*)((ctron_list*)(t_cx_of(t_a)))->items[0]))));
return 0;
}
ctron_list* t_env_at(ctron_list* t_env, const char* t_nm) 
{
int32_t t_i = 0;
{
const char* t_en = 0;
while ((t_i < ((ctron_list*)(t_env))->n)) {
t_en = ((const char*)((ctron_list*)(t_env))->items[t_i]);
if (t_seq2(((const char*)((ctron_list*)(t_en))->items[0]), t_nm)) {
{
return t_en;
}
}
t_i += 1;
}
}
return ctron_list_new();
}
ctron_list* t_env_add(ctron_list* t_env, const char* t_nm, t_Val t_val) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
ctron_list* t_en = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_en), t_nm);
ctron_list_push((ctron_list*)(t_en), (char*)(t_v_box(t_val)));
ctron_list_push((ctron_list*)(t_out), (char*)(t_en));
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_env))->n)) {
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_env))->items[t_i]));
t_i += 1;
}
return t_out;
}
ctron_list* t_env_set(ctron_list* t_env, const char* t_nm, t_Val t_val) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
int t_done = 0;
int32_t t_i = 0;
{
const char* t_en = 0;
while ((t_i < ((ctron_list*)(t_env))->n)) {
t_en = ((const char*)((ctron_list*)(t_env))->items[t_i]);
if (((!t_done) && t_seq2(((const char*)((ctron_list*)(t_en))->items[0]), t_nm))) {
{
ctron_list* t_nb = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_nb), t_nm);
ctron_list_push((ctron_list*)(t_nb), (char*)(t_v_box(t_val)));
ctron_list_push((ctron_list*)(t_out), (char*)(t_nb));
t_done = 1;
}
}
else {
{
ctron_list_push((ctron_list*)(t_out), t_en);
}
}
t_i += 1;
}
}
return t_out;
}
t_Val t_env_at_val(ctron_list* t_env, const char* t_nm) 
{
ctron_list* t_en = (ctron_list*)(t_env_at(t_env, t_nm));
if ((((ctron_list*)(t_en))->n > 0)) {
{
return t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_en))->items[1])));
}
}
return t_vV2();
}
ctron_list* t_env_drop(ctron_list* t_env, int32_t t_n) 
{
ctron_list* t_out = (ctron_list*)(ctron_list_new());
int32_t t_i = t_n;
while ((t_i < ((ctron_list*)(t_env))->n)) {
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_env))->items[t_i]));
t_i += 1;
}
return t_out;
}
ctron_list* t_env_dedupe(ctron_list* t_env, int32_t t_n) 
{
ctron_list* t_keep = (ctron_list*)(ctron_list_new());
int32_t t_i = 0;
{
const char* t_en = 0;
int t_seen = 0;
while (((t_i < t_n) && (t_i < ((ctron_list*)(t_env))->n))) {
t_en = ((const char*)((ctron_list*)(t_env))->items[t_i]);
t_seen = 0;
int32_t t_j = 0;
while ((t_j < ((ctron_list*)(t_keep))->n)) {
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_keep))->items[t_j])))->items[0]), ((const char*)((ctron_list*)(t_en))->items[0]))) {
{
t_seen = 1;
}
}
t_j += 1;
}
if ((!t_seen)) {
{
ctron_list_push((ctron_list*)(t_keep), t_en);
}
}
t_i += 1;
}
}
ctron_list* t_out = (ctron_list*)(t_keep);
t_i = t_n;
while ((t_i < ((ctron_list*)(t_env))->n)) {
ctron_list_push((ctron_list*)(t_out), ((const char*)((ctron_list*)(t_env))->items[t_i]));
t_i += 1;
}
return t_out;
}
t_Flow t_e4(const char* t_fl, ctron_list* t_env, t_Val t_vv, const char* t_out) 
{
return (t_Flow){.kind = t_fl, .env = t_env, .v = t_vv, .out = t_out};
}
t_Flow t_s4(const char* t_fl, ctron_list* t_env, const char* t_out, t_Val t_vv) 
{
return (t_Flow){.kind = t_fl, .env = t_env, .v = t_vv, .out = t_out};
}
ctron_list* t_expr_of_text(const char* t_s) 
{
ctron_list* t_toks = (ctron_list*)(ctron_list_new());
ctron_list* t_lns = (ctron_list*)(ctron_list_new());
t_scan(t_s, t_toks, t_lns);
ctron_cell* t_cur = (ctron_cell*)(ctron_cell_new(0));
return t_p_stmt_expr(t_toks, t_cur, t_lns, "1");
}
ctron_list* t_find_decl(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
const char* t_tt = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
t_tt = ((const char*)((ctron_list*)(t_d))->items[0]);
if (t_or3(t_seq2(t_tt, "Fn"), t_seq2(t_tt, "FnPub"), t_seq2(t_tt, "FnC"))) {
{
if (t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm)) {
{
return t_d;
}
}
}
}
t_i += 1;
}
}
return ctron_list_new();
}
const char* t_unesc(const char* t_s) 
{
const char* t_out = "";
int32_t t_i = 0;
int32_t t_n = strlen((const char*)(t_s));
{
int32_t t_c = 0;
while ((t_i < t_n)) {
t_c = ctron_byte_at(t_s, t_i);
if (((t_c == 92) && ((t_i + 1) < t_n))) {
{
int32_t t_e = ctron_byte_at(t_s, (t_i + 1));
if ((t_e == 110)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
t_i += 2;
}
}
else {
if ((t_e == 116)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\t"));
t_i += 2;
}
}
else {
if ((t_e == 114)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\r"));
t_i += 2;
}
}
else {
if ((t_e == 123)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("{"));
t_i += 2;
}
}
else {
if ((t_e == 34)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\""));
t_i += 2;
}
}
else {
if ((t_e == 92)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\\"));
t_i += 2;
}
}
else {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_i, (t_i + 2))));
t_i += 2;
}
}
}
}
}
}
}
}
}
else {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_s, t_i, (t_i + 1))));
t_i += 1;
}
}
}
}
return t_out;
}
t_Flow t_eval_str(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e) 
{
const char* t_parts = ((const char*)((ctron_list*)(t_e))->items[1]);
const char* t_acc = "";
ctron_list* t_env2 = (ctron_list*)(t_env);
int32_t t_j = 1;
{
const char* t_p = 0;
while ((t_j < ctron_len((const void*)(t_parts)))) {
t_p = ((const char*)((ctron_list*)(t_parts))->items[t_j]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_p))->items[0])), (const char*)("Text")) == 0)) {
{
t_acc = ctron_str_concat((const char*)(t_acc), (const char*)(t_unesc(((const char*)((ctron_list*)(t_p))->items[1]))));
}
}
else {
{
ctron_list* t_ex = (ctron_list*)(t_expr_of_text(((const char*)((ctron_list*)(t_p))->items[1])));
t_Flow t_er = t_eval_expr(t_file, t_env2, t_out, t_ex);
t_out = (t_er).out;
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
t_env2 = (ctron_list*)((t_er).env);
t_acc = ctron_str_concat((const char*)(t_acc), (const char*)(t_fmt((t_er).v)));
}
}
t_j += 1;
}
}
return t_e4("k", t_env2, t_vS(t_acc), t_out);
}
ctron_list* t_pm2(const char* t_ok, ctron_list* t_env) 
{
ctron_list* t_r = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_r), t_ok);
ctron_list_push((ctron_list*)(t_r), (char*)(t_env));
return t_r;
}
int t_enum_var(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Enum")) == 0)) {
{
const char* t_vs = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(t_vs)))) {
if (t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vs))->items[t_k])))->items[1]), t_nm)) {
{
return 1;
}
}
t_k += 1;
}
}
}
t_i += 1;
}
}
return 0;
}
const char* t_ty_head(ctron_list* t_ty) 
{
const char* t_t = ((const char*)((ctron_list*)(t_ty))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Named")) == 0)) {
{
return ((const char*)((ctron_list*)(t_ty))->items[1]);
}
}
if ((((ctron_list*)(t_ty))->n > 1)) {
{
return t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_ty))->items[1])));
}
}
return "";
}
ctron_list* t_find_trait(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Trait")) == 0) && t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm))) {
{
return t_d;
}
}
t_i += 1;
}
}
return ctron_list_new();
}
int t_is_class(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Class")) == 0) && t_seq2(((const char*)((ctron_list*)(t_d))->items[1]), t_nm))) {
{
return 1;
}
}
t_i += 1;
}
}
return 0;
}
ctron_list* t_find_impl_method(ctron_list* t_file, const char* t_cls, const char* t_m) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0) && t_seq2(t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]))), t_cls))) {
{
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_k = 1;
{
const char* t_it = 0;
while ((t_k < ctron_len((const void*)(t_its)))) {
t_it = ((const char*)((ctron_list*)(t_its))->items[t_k]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_it))->items[0])), (const char*)("Method")) == 0) && t_seq2(((const char*)((ctron_list*)(t_it))->items[1]), t_m))) {
{
return t_it;
}
}
t_k += 1;
}
}
const char* t_tn = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])));
if ((strlen((const char*)(t_tn)) > 0)) {
{
ctron_list* t_tr = (ctron_list*)(t_find_trait(t_file, t_tn));
if ((((ctron_list*)(t_tr))->n > 0)) {
{
const char* t_tits = ((const char*)((ctron_list*)(t_tr))->items[4]);
int32_t t_j = 1;
{
const char* t_tit = 0;
while ((t_j < ctron_len((const void*)(t_tits)))) {
t_tit = ((const char*)((ctron_list*)(t_tits))->items[t_j]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_tit))->items[0])), (const char*)("Method")) == 0) && t_seq2(((const char*)((ctron_list*)(t_tit))->items[1]), t_m)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tit))->items[5])))->items[0])), (const char*)("None")) != 0))) {
{
return t_tit;
}
}
t_j += 1;
}
}
}
}
}
}
}
}
t_i += 1;
}
}
return ctron_list_new();
}
ctron_list* t_find_impl_prop(ctron_list* t_file, const char* t_cls, const char* t_m) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Impl")) == 0) && t_seq2(t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3]))), t_cls))) {
{
const char* t_its = ((const char*)((ctron_list*)(t_d))->items[4]);
int32_t t_k = 1;
{
const char* t_it = 0;
while ((t_k < ctron_len((const void*)(t_its)))) {
t_it = ((const char*)((ctron_list*)(t_its))->items[t_k]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_it))->items[0])), (const char*)("Prop")) == 0) && t_seq2(((const char*)((ctron_list*)(t_it))->items[1]), t_m)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_it))->items[3])))->items[0])), (const char*)("None")) != 0))) {
{
return t_it;
}
}
t_k += 1;
}
}
}
}
t_i += 1;
}
}
return ctron_list_new();
}
t_Flow t_call_method_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_mth, ctron_list* t_sv, ctron_list* t_vals) 
{
const char* t_ps = ((const char*)((ctron_list*)(t_mth))->items[3]);
ctron_list* t_nenv = (ctron_list*)(t_env_add(ctron_list_new(), "self", t_bind_of(t_file, t_v_unbox(t_sv))));
int32_t t_k = 1;
int32_t t_ai = 0;
{
const char* t_pr = 0;
while ((t_k < ctron_len((const void*)(t_ps)))) {
t_pr = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pr))->items[0])), (const char*)("Param")) == 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_pr))->items[2]), "self")) {
{
t_nenv = (ctron_list*)(t_env_add(t_nenv, "self", t_bind_of(t_file, t_v_unbox(t_sv))));
}
}
else {
{
t_nenv = (ctron_list*)(t_env_add(t_nenv, ((const char*)((ctron_list*)(t_pr))->items[2]), t_bind_of(t_file, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[t_ai]))))));
t_ai += 1;
}
}
}
}
t_k += 1;
}
}
t_Flow t_br = t_run_block(t_file, t_nenv, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_mth))->items[5])), 0);
const char* t_fl = "k";
if ((strcmp((const char*)((t_br).kind), (const char*)("a")) == 0)) {
{
t_fl = "a";
}
}
return t_e4(t_fl, t_env, (t_br).v, (t_br).out);
}
t_Flow t_call_prop_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_pp, ctron_list* t_sv) 
{
ctron_list* t_nenv = (ctron_list*)(t_env_add(ctron_list_new(), "self", (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_sv)}));
t_Flow t_br = t_run_block(t_file, t_nenv, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_pp))->items[3])), 0);
const char* t_fl = "k";
if ((strcmp((const char*)((t_br).kind), (const char*)("a")) == 0)) {
{
t_fl = "a";
}
}
return t_e4(t_fl, t_env, (t_br).v, (t_br).out);
}
t_Val t_bind_of(ctron_list* t_file, t_Val t_v) 
{
if (((t_v).tag == 8)) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "U") && (!t_is_class(t_file, ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]))))) {
{
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_vdeep(t_cx_of(t_v)))};
}
}
}
}
return t_v;
}
int t_u_set_ip(ctron_list* t_u, const char* t_nm, ctron_list* t_nv) 
{
int32_t t_i = 2;
while (((t_i + 1) < ((ctron_list*)(t_u))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_u))->items[t_i]), t_nm)) {
{
((ctron_list*)(t_u))->items[(t_i + 1)] = (char*)(t_nv);
return 1;
}
}
t_i += 2;
}
return 0;
}
ctron_list* t_u_field(ctron_list* t_u, const char* t_nm) 
{
int32_t t_i = 2;
while (((t_i + 1) < ((ctron_list*)(t_u))->n)) {
if (t_seq2(((const char*)((ctron_list*)(t_u))->items[t_i]), t_nm)) {
{
return ((const char*)((ctron_list*)(t_u))->items[(t_i + 1)]);
}
}
t_i += 2;
}
return ctron_list_new();
}
ctron_list* t_pat_match(ctron_list* t_pat, t_Val t_v, ctron_list* t_env) 
{
const char* t_t = ((const char*)((ctron_list*)(t_pat))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("PatOr")) == 0)) {
{
int32_t t_k5 = 1;
while ((t_k5 < ((ctron_list*)(t_pat))->n)) {
ctron_list* t_pr9 = (ctron_list*)(t_pat_match((ctron_list*)(((const char*)((ctron_list*)(t_pat))->items[t_k5])), t_v, t_env));
if (t_seq2(((const char*)((ctron_list*)(t_pr9))->items[0]), "1")) {
{
return t_pr9;
}
}
t_k5 += 1;
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatWild")) == 0)) {
{
return t_pm2("1", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatLitI")) == 0)) {
{
if ((((t_v).tag == 1) && ((int32_t)((t_v).iv) == t_dvi(((const char*)((ctron_list*)(t_pat))->items[1]))))) {
{
return t_pm2("1", t_env);
}
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatLitS")) == 0)) {
{
if ((((t_v).tag == 5) && t_seq2((t_v).s, ((const char*)((ctron_list*)(t_pat))->items[1])))) {
{
return t_pm2("1", t_env);
}
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatLitB")) == 0)) {
{
if ((((t_v).tag == 4) && ((t_v).b == t_seq2(((const char*)((ctron_list*)(t_pat))->items[1]), "true")))) {
{
return t_pm2("1", t_env);
}
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatAgg")) == 0)) {
{
const char* t_sub = ((const char*)((ctron_list*)(t_pat))->items[2]);
if ((((((t_v).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "U")) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]), ((const char*)((ctron_list*)(t_pat))->items[1]))) && (strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("SubSt")) == 0))) {
{
ctron_list* t_env2 = (ctron_list*)(t_env);
int t_ok3 = 1;
int32_t t_k = 1;
{
const char* t_pf = 0;
const char* t_nm2 = 0;
while (((t_k < ctron_len((const void*)(t_sub))) && t_ok3)) {
t_pf = ((const char*)((ctron_list*)(t_sub))->items[t_k]);
t_nm2 = ((const char*)((ctron_list*)(t_pf))->items[1]);
ctron_list* t_fv2 = (ctron_list*)(t_u_field(t_cx_of(t_v), t_nm2));
if ((((ctron_list*)(t_fv2))->n == 0)) {
{
t_ok3 = 0;
}
}
else {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_pf))->items[2])))->items[0])), (const char*)("None")) == 0)) {
{
t_env2 = (ctron_list*)(t_env_add(t_env2, t_nm2, t_v_unbox(t_fv2)));
}
}
else {
{
ctron_list* t_pr3 = (ctron_list*)(t_pat_match((ctron_list*)(((const char*)((ctron_list*)(t_pf))->items[2])), t_v_unbox(t_fv2), t_env2));
if ((!t_seq2(((const char*)((ctron_list*)(t_pr3))->items[0]), "1"))) {
{
t_ok3 = 0;
}
}
else {
{
t_env2 = (ctron_list*)(((const char*)((ctron_list*)(t_pr3))->items[1]));
}
}
}
}
}
}
t_k += 1;
}
}
if (t_ok3) {
{
return t_pm2("1", t_env2);
}
}
return t_pm2("0", t_env);
}
}
if (((((t_v).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "T")) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]), ((const char*)((ctron_list*)(t_pat))->items[1])))) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("SubUnit")) == 0)) {
{
if ((((ctron_list*)(t_cx_of(t_v)))->n == 2)) {
{
return t_pm2("1", t_env);
}
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_sub))->items[0])), (const char*)("SubTup")) == 0)) {
{
int32_t t_np = (ctron_len((const void*)(t_sub)) - 1);
int32_t t_npv = (((ctron_list*)(t_cx_of(t_v)))->n - 2);
if ((t_np == t_npv)) {
{
ctron_list* t_env3 = (ctron_list*)(t_env);
int t_ok2 = 1;
int32_t t_k = 1;
{
const char* t_pp = 0;
while (((t_k < ctron_len((const void*)(t_sub))) && t_ok2)) {
t_pp = ((const char*)((ctron_list*)(t_sub))->items[t_k]);
t_Val t_pv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[(t_k + 1)])));
ctron_list* t_pr2 = (ctron_list*)(t_pat_match((ctron_list*)(t_pp), t_pv, t_env3));
if ((!t_seq2(((const char*)((ctron_list*)(t_pr2))->items[0]), "1"))) {
{
t_ok2 = 0;
}
}
else {
{
t_env3 = (ctron_list*)(((const char*)((ctron_list*)(t_pr2))->items[1]));
}
}
t_k += 1;
}
}
if (t_ok2) {
{
return t_pm2("1", t_env3);
}
}
}
}
return t_pm2("0", t_env);
}
}
}
}
return t_pm2("0", t_env);
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatId")) == 0)) {
{
return t_pm2("1", t_env_add(t_env, ((const char*)((ctron_list*)(t_pat))->items[1]), t_v));
}
}
if ((strcmp((const char*)(t_t), (const char*)("PatTup")) == 0)) {
{
if ((!(((t_v).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "P")))) {
{
return t_pm2("0", t_env);
}
}
if (((((ctron_list*)(t_pat))->n - 1) != (((ctron_list*)(t_cx_of(t_v)))->n - 1))) {
{
return t_pm2("0", t_env);
}
}
ctron_list* t_env4 = (ctron_list*)(t_env);
int32_t t_k4 = 1;
while ((t_k4 < ((ctron_list*)(t_pat))->n)) {
ctron_list* t_pr4 = (ctron_list*)(t_pat_match((ctron_list*)(((const char*)((ctron_list*)(t_pat))->items[t_k4])), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[t_k4]))), t_env4));
if ((!t_seq2(((const char*)((ctron_list*)(t_pr4))->items[0]), "1"))) {
{
return t_pm2("0", t_env);
}
}
t_env4 = (ctron_list*)(((const char*)((ctron_list*)(t_pr4))->items[1]));
t_k4 += 1;
}
return t_pm2("1", t_env4);
}
}
return t_pm2("0", t_env);
}
t_Flow t_eval_expr(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e) 
{
const char* t_t = ((const char*)((ctron_list*)(t_e))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Int")) == 0)) {
{
if ((((ctron_list*)(t_e))->n > 2)) {
{
const char* t_sfx = ((const char*)((ctron_list*)(t_e))->items[2]);
if (t_or2((strcmp((const char*)(t_sfx), (const char*)("u8")) == 0), t_or2((strcmp((const char*)(t_sfx), (const char*)("i8")) == 0), t_or2((strcmp((const char*)(t_sfx), (const char*)("u16")) == 0), (strcmp((const char*)(t_sfx), (const char*)("i16")) == 0))))) {
{
return t_e4("k", t_env, t_vW(t_sfx, t_txt_num(((const char*)((ctron_list*)(t_e))->items[1]))), t_out);
}
}
if (t_or2((strcmp((const char*)(t_sfx), (const char*)("u64")) == 0), (strcmp((const char*)(t_sfx), (const char*)("usize")) == 0))) {
{
return t_e4("k", t_env, t_v7(t_lit_to_dec(((const char*)((ctron_list*)(t_e))->items[1]))), t_out);
}
}
}
}
if (t_or2((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[2]))) == 0), t_or2(t_seq2(((const char*)((ctron_list*)(t_e))->items[2]), "i64"), t_seq2(((const char*)((ctron_list*)(t_e))->items[2]), "isize")))) {
{
if (t_lit_wide_i32(((const char*)((ctron_list*)(t_e))->items[1]))) {
{
return t_e4("k", t_env, t_v6(t_lit_to_dec(((const char*)((ctron_list*)(t_e))->items[1]))), t_out);
}
}
}
}
return t_e4("k", t_env, t_vI(t_txt_num(((const char*)((ctron_list*)(t_e))->items[1]))), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Bool")) == 0)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "true")) {
{
return t_e4("k", t_env, t_vB(1), t_out);
}
}
return t_e4("k", t_env, t_vB(0), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Float")) == 0)) {
{
return t_e4("k", t_env, t_vD(t_df_can(((const char*)((ctron_list*)(t_e))->items[1]))), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Str")) == 0)) {
{
return t_eval_str(t_file, t_env, t_out, t_e);
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Ident")) == 0)) {
{
ctron_list* t_en = (ctron_list*)(t_env_at(t_env, ((const char*)((ctron_list*)(t_e))->items[1])));
if ((((ctron_list*)(t_en))->n > 0)) {
{
return t_e4("k", t_env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_en))->items[1]))), t_out);
}
}
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Some"), t_or2(t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "None"), t_or2(t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Ok"), t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Err"))))) {
{
ctron_list* t_tv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv), "T");
ctron_list_push((ctron_list*)(t_tv), ((const char*)((ctron_list*)(t_e))->items[1]));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv)}, t_out);
}
}
if (t_enum_var(t_file, ((const char*)((ctron_list*)(t_e))->items[1]))) {
{
ctron_list* t_tv2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv2), "T");
ctron_list_push((ctron_list*)(t_tv2), ((const char*)((ctron_list*)(t_e))->items[1]));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv2)}, t_out);
}
}
ctron_list* t_fnd = (ctron_list*)(t_find_decl(t_file, ((const char*)((ctron_list*)(t_e))->items[1])));
if ((((ctron_list*)(t_fnd))->n > 0)) {
{
ctron_list* t_fv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_fv), "F");
ctron_list_push((ctron_list*)(t_fv), ((const char*)((ctron_list*)(t_e))->items[1]));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_fv)}, t_out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "parallel")) {
{
ctron_list* t_ns = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ns), "NS");
ctron_list_push((ctron_list*)(t_ns), "parallel");
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_ns)}, t_out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "bit")) {
{
ctron_list* t_nsb = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_nsb), "NS");
ctron_list_push((ctron_list*)(t_nsb), "bit");
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_nsb)}, t_out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "dom")) {
{
ctron_list* t_ns = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ns), "NS");
ctron_list_push((ctron_list*)(t_ns), "dom");
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_ns)}, t_out);
}
}
ctron_panic(ctron_str_concat((const char*)("unbound:"), (const char*)(((const char*)((ctron_list*)(t_e))->items[1]))));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Unary")) == 0)) {
{
t_Flow t_ur = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_ur).kind), (const char*)("k")) != 0)) {
{
return t_ur;
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[1]), "Neg")) {
{
if ((((t_ur).v).tag == 6)) {
{
const char* t_dt = ((t_ur).v).s;
if ((strcmp((const char*)(t_dt), (const char*)("0")) != 0)) {
{
if ((ctron_byte_at(t_dt, 0) == 45)) {
{
t_dt = ctron_byte_slice(t_dt, 1, strlen((const char*)(t_dt)));
}
}
else {
{
t_dt = ctron_str_concat((const char*)("-"), (const char*)(t_dt));
}
}
}
}
return t_e4("k", (t_ur).env, t_vD(t_dt), (t_ur).out);
}
}
if ((((t_ur).v).tag == 2)) {
{
if ((strcmp((const char*)(((t_ur).v).s), (const char*)("")) == 0)) {
{
int64_t t_z64 = 0;
return t_e4("k", (t_ur).env, t_v6i(ctron_i64_sub((int64_t)(t_z64), (int64_t)(((t_ur).v).iv))), (t_ur).out);
}
}
const char* t_nt6 = t_c6can(ctron_str_concat((const char*)("-"), (const char*)(t_v6t((t_ur).v))));
if ((!t_c6_in_i64(t_nt6))) {
{
ctron_panic("integer overflow (-)");
}
}
return t_e4("k", (t_ur).env, t_v6(t_nt6), (t_ur).out);
}
}
return t_e4("k", (t_ur).env, t_vI((0 - (int32_t)(((t_ur).v).iv))), (t_ur).out);
}
}
return t_e4("k", (t_ur).env, t_vB((!t_truth((t_ur).v))), (t_ur).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Binary")) == 0)) {
{
const char* t_op = ((const char*)((ctron_list*)(t_e))->items[1]);
if (t_seq2(t_op, "OrOr")) {
{
t_Flow t_lor = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_lor).kind), (const char*)("k")) != 0)) {
{
return t_lor;
}
}
if (t_truth((t_lor).v)) {
{
return t_e4("k", (t_lor).env, t_vB(1), (t_lor).out);
}
}
t_Flow t_ror = t_eval_expr(t_file, (t_lor).env, (t_lor).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
if ((strcmp((const char*)((t_ror).kind), (const char*)("k")) != 0)) {
{
return t_ror;
}
}
return t_e4("k", (t_ror).env, t_vB(t_truth((t_ror).v)), (t_ror).out);
}
}
if (t_seq2(t_op, "AndAnd")) {
{
t_Flow t_lr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_lr).kind), (const char*)("k")) != 0)) {
{
return t_lr;
}
}
if ((!t_truth((t_lr).v))) {
{
return t_e4("k", (t_lr).env, t_vB(0), (t_lr).out);
}
}
t_Flow t_rr = t_eval_expr(t_file, (t_lr).env, (t_lr).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
if ((strcmp((const char*)((t_rr).kind), (const char*)("k")) != 0)) {
{
return t_rr;
}
}
return t_e4("k", (t_rr).env, t_vB(t_truth((t_rr).v)), (t_rr).out);
}
}
if (t_seq2(t_op, "Or")) {
{
t_Flow t_lo = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_lo).kind), (const char*)("k")) != 0)) {
{
return t_lo;
}
}
t_Val t_lv = (t_lo).v;
if (((t_lv).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_lv)))->items[0]), "T")) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_lv)))->items[1]), "Some"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_lv)))->items[1]), "Ok"))) {
{
if ((((ctron_list*)(t_cx_of(t_lv)))->n == 3)) {
{
return t_e4("k", (t_lo).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_lv)))->items[2]))), (t_lo).out);
}
}
return t_e4("k", (t_lo).env, t_lv, (t_lo).out);
}
}
}
}
}
}
t_Flow t_ro = t_eval_expr(t_file, (t_lo).env, (t_lo).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
if ((strcmp((const char*)((t_ro).kind), (const char*)("k")) != 0)) {
{
return t_ro;
}
}
return t_e4("k", (t_ro).env, (t_ro).v, (t_ro).out);
}
}
t_Flow t_lr2 = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_lr2).kind), (const char*)("k")) != 0)) {
{
return t_lr2;
}
}
t_Flow t_rr2 = t_eval_expr(t_file, (t_lr2).env, (t_lr2).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
if ((strcmp((const char*)((t_rr2).kind), (const char*)("k")) != 0)) {
{
return t_rr2;
}
}
t_Val t_a = (t_lr2).v;
t_Val t_b = (t_rr2).v;
if (t_seq2(t_op, "Eq")) {
{
return t_e4("k", (t_rr2).env, t_vB(t_veq(t_a, t_b)), (t_rr2).out);
}
}
if (t_seq2(t_op, "Ne")) {
{
return t_e4("k", (t_rr2).env, t_vB((!t_veq(t_a, t_b))), (t_rr2).out);
}
}
if (t_seq2(t_op, "Lt")) {
{
return t_e4("k", (t_rr2).env, t_vB((t_vcmp(t_a, t_b) < 0)), (t_rr2).out);
}
}
if (t_seq2(t_op, "Gt")) {
{
return t_e4("k", (t_rr2).env, t_vB((t_vcmp(t_a, t_b) > 0)), (t_rr2).out);
}
}
if (t_seq2(t_op, "Le")) {
{
return t_e4("k", (t_rr2).env, t_vB((t_vcmp(t_a, t_b) <= 0)), (t_rr2).out);
}
}
if (t_seq2(t_op, "Ge")) {
{
return t_e4("k", (t_rr2).env, t_vB((t_vcmp(t_a, t_b) >= 0)), (t_rr2).out);
}
}
if (t_seq2(t_op, "Add")) {
{
if ((((t_a).tag == 5) && ((t_b).tag == 5))) {
{
return t_e4("k", (t_rr2).env, t_vS(ctron_str_concat((const char*)(t_fmt(t_a)), (const char*)(t_fmt(t_b)))), (t_rr2).out);
}
}
return t_e4("k", (t_rr2).env, t_val_arith(t_op, t_a, t_b), (t_rr2).out);
}
}
if (t_or3(t_seq2(t_op, "Sub"), t_seq2(t_op, "Mul"), t_seq2(t_op, "Div"))) {
{
return t_e4("k", (t_rr2).env, t_val_arith(t_op, t_a, t_b), (t_rr2).out);
}
}
if (t_seq2(t_op, "Mod")) {
{
return t_e4("k", (t_rr2).env, t_val_arith(t_op, t_a, t_b), (t_rr2).out);
}
}
if (t_or2(t_seq2(t_op, "WrapAdd"), t_seq2(t_op, "WrapSub"))) {
{
return t_e4("k", (t_rr2).env, t_val_arith(t_op, t_a, t_b), (t_rr2).out);
}
}
ctron_panic(ctron_str_concat((const char*)("binop:"), (const char*)(t_op)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Range")) == 0)) {
{
t_Flow t_fr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_fr).kind), (const char*)("k")) != 0)) {
{
return t_fr;
}
}
t_Flow t_tr = t_eval_expr(t_file, (t_fr).env, (t_fr).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[3])));
if ((strcmp((const char*)((t_tr).kind), (const char*)("k")) != 0)) {
{
return t_tr;
}
}
ctron_list* t_rv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_rv), "R");
ctron_list_push((ctron_list*)(t_rv), ((const char*)((ctron_list*)(t_e))->items[1]));
ctron_list_push((ctron_list*)(t_rv), (char*)(t_v_box((t_fr).v)));
ctron_list_push((ctron_list*)(t_rv), (char*)(t_v_box((t_tr).v)));
return t_e4("k", (t_tr).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_rv)}, (t_tr).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Member")) == 0)) {
{
t_Flow t_ob = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)((t_ob).kind), (const char*)("k")) != 0)) {
{
return t_ob;
}
}
t_Val t_recv = (t_ob).v;
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[2]), "TIdx")) {
{
const char* t_ti = ((const char*)((ctron_list*)(t_e))->items[3]);
if (((t_recv).tag == 8)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0])), (const char*)("P")) == 0)) {
{
int32_t t_pi2 = t_dvi(t_ti);
if (((t_pi2 + 1) < ((ctron_list*)(t_cx_of(t_recv)))->n)) {
{
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[(t_pi2 + 1)]))), (t_ob).out);
}
}
}
}
}
}
ctron_panic(ctron_str_concat((const char*)("tindex:"), (const char*)(t_ti)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_e))->items[2]), "Nm")) {
{
const char* t_m = ((const char*)((ctron_list*)(t_e))->items[3]);
if (t_seq2(t_m, "len")) {
{
if (((t_recv).tag == 5)) {
{
return t_e4("k", (t_ob).env, t_vI(strlen((const char*)((t_recv).s))), (t_ob).out);
}
}
if (((t_recv).tag == 8)) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "A"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "L"))) {
{
return t_e4("k", (t_ob).env, t_vI((((ctron_list*)(t_cx_of(t_recv)))->n - 1)), (t_ob).out);
}
}
}
}
if (((t_recv).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "SB")) {
{
int32_t t_sbl = 0;
int32_t t_sbi = 1;
while ((t_sbi < ((ctron_list*)(t_cx_of(t_recv)))->n)) {
t_sbl += ctron_len((const void*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[t_sbi])));
t_sbi += 1;
}
return t_e4("k", (t_ob).env, t_vI(t_sbl), (t_ob).out);
}
}
}
}
const char* t_tn = "?";
if (((t_recv).tag == 8)) {
{
if ((((ctron_list*)(t_cx_of(t_recv)))->n > 1)) {
{
t_tn = ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]);
}
}
}
}
const char* t_en = "?";
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_e))->items[1]))) > 1)) {
{
t_en = ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("/"))), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1])));
}
}
ctron_panic(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("len target recv="), (const char*)(t_recv_cname(t_recv)))), (const char*)("/"))), (const char*)(t_tn))), (const char*)(" m="))), (const char*)(t_m))), (const char*)(" recvexpr="))), (const char*)(t_en)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((((t_recv).tag == 5) && t_seq2(t_m, "char_len"))) {
{
int32_t t_cl = 0;
int32_t t_ci = 0;
while ((t_ci < strlen((const char*)((t_recv).s)))) {
if ((!t_utf8_cont((t_recv).s, t_ci))) {
{
t_cl += 1;
}
}
t_ci += 1;
}
return t_e4("k", (t_ob).env, t_vI(t_cl), (t_ob).out);
}
}
if (((t_recv).tag == 8)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0])), (const char*)("BOX")) == 0)) {
{
t_Val t_ivb = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1])));
if (((t_ivb).tag == 8)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_ivb)))->items[0])), (const char*)("U")) == 0)) {
{
ctron_list* t_fv3 = (ctron_list*)(t_u_field(t_cx_of(t_ivb), t_m));
if ((((ctron_list*)(t_fv3))->n > 0)) {
{
return t_e4("k", (t_ob).env, t_v_unbox(t_fv3), (t_ob).out);
}
}
}
}
}
}
}
}
if (((((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0])), (const char*)("P")) == 0) && (ctron_len((const void*)(t_m)) > 0)) && (ctron_byte_at(t_m, 0) >= 48)) && (ctron_byte_at(t_m, 0) <= 57))) {
{
int32_t t_pi = t_dvi(t_m);
if (((t_pi + 1) < ((ctron_list*)(t_cx_of(t_recv)))->n)) {
{
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[(t_pi + 1)]))), (t_ob).out);
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0])), (const char*)("U")) == 0)) {
{
ctron_list* t_fv = (ctron_list*)(t_u_field(t_cx_of(t_recv), t_m));
if ((((ctron_list*)(t_fv))->n > 0)) {
{
return t_e4("k", (t_ob).env, t_v_unbox(t_fv), (t_ob).out);
}
}
if (t_is_class(t_file, ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]))) {
{
ctron_list* t_pp = (ctron_list*)(t_find_impl_prop(t_file, ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), t_m));
if ((((ctron_list*)(t_pp))->n > 0)) {
{
t_Flow t_prr = t_call_prop_vals(t_file, (t_ob).env, (t_ob).out, t_pp, t_cx_of(t_recv));
return t_e4((t_prr).kind, (t_ob).env, (t_prr).v, (t_prr).out);
}
}
}
}
ctron_panic(ctron_str_concat((const char*)("field:"), (const char*)(t_m)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
}
}
ctron_panic(ctron_str_concat((const char*)("member:"), (const char*)(t_m)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
ctron_panic("tindex");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("If")) == 0)) {
{
t_Flow t_cr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)((t_cr).kind), (const char*)("k")) != 0)) {
{
return t_cr;
}
}
if (t_truth((t_cr).v)) {
{
t_Flow t_br = t_run_block(t_file, (t_cr).env, (t_cr).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), 0);
return t_e4((t_br).kind, (t_br).env, (t_br).v, (t_br).out);
}
}
const char* t_el = ((const char*)((ctron_list*)(t_e))->items[3]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_el))->items[0])), (const char*)("None")) == 0)) {
{
return t_e4("k", (t_cr).env, t_vV2(), (t_cr).out);
}
}
return t_eval_expr(t_file, (t_cr).env, (t_cr).out, (ctron_list*)(t_el));
}
}
if ((strcmp((const char*)(t_t), (const char*)("BlockExpr")) == 0)) {
{
t_Flow t_br = t_run_block(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])), 0);
return t_e4((t_br).kind, (t_br).env, (t_br).v, (t_br).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Closure")) == 0)) {
{
ctron_list* t_cv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_cv), "C");
ctron_list_push((ctron_list*)(t_cv), ((const char*)((ctron_list*)(t_e))->items[1]));
ctron_list_push((ctron_list*)(t_cv), ((const char*)((ctron_list*)(t_e))->items[3]));
ctron_list_push((ctron_list*)(t_cv), (char*)(t_env));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_cv)}, t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("TupleE")) == 0)) {
{
ctron_list* t_tp = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tp), "P");
ctron_list* t_env5 = (ctron_list*)(t_env);
const char* t_out5 = t_out;
int32_t t_k5 = 1;
while ((t_k5 < ((ctron_list*)(t_e))->n)) {
t_Flow t_er5 = t_eval_expr(t_file, t_env5, t_out5, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_k5])));
if ((strcmp((const char*)((t_er5).kind), (const char*)("k")) != 0)) {
{
return t_er5;
}
}
t_env5 = (ctron_list*)((t_er5).env);
t_out5 = (t_er5).out;
ctron_list_push((ctron_list*)(t_tp), (char*)(t_v_box((t_er5).v)));
t_k5 += 1;
}
return t_e4("k", t_env5, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tp)}, t_out5);
}
}
if ((strcmp((const char*)(t_t), (const char*)("ArrLit")) == 0)) {
{
ctron_list* t_arr = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_arr), "A");
ctron_list* t_env2 = (ctron_list*)(t_env);
const char* t_out2 = t_out;
int32_t t_k = 1;
while ((t_k < ((ctron_list*)(t_e))->n)) {
t_Flow t_er = t_eval_expr(t_file, t_env2, t_out2, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[t_k])));
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
t_env2 = (ctron_list*)((t_er).env);
t_out2 = (t_er).out;
ctron_list_push((ctron_list*)(t_arr), (char*)(t_v_box((t_er).v)));
t_k += 1;
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_arr)}, t_out2);
}
}
if ((strcmp((const char*)(t_t), (const char*)("StructLit")) == 0)) {
{
ctron_list* t_su = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_su), "U");
ctron_list_push((ctron_list*)(t_su), ((const char*)((ctron_list*)(t_e))->items[1]));
const char* t_lf = ((const char*)((ctron_list*)(t_e))->items[2]);
ctron_list* t_env2 = (ctron_list*)(t_env);
const char* t_out2 = t_out;
int32_t t_k = 1;
{
const char* t_fld = 0;
while ((t_k < ctron_len((const void*)(t_lf)))) {
t_fld = ((const char*)((ctron_list*)(t_lf))->items[t_k]);
t_Flow t_fv = t_eval_expr(t_file, t_env2, t_out2, (ctron_list*)(((const char*)((ctron_list*)(t_fld))->items[2])));
if ((strcmp((const char*)((t_fv).kind), (const char*)("k")) != 0)) {
{
return t_fv;
}
}
t_env2 = (ctron_list*)((t_fv).env);
t_out2 = (t_fv).out;
ctron_list_push((ctron_list*)(t_su), ((const char*)((ctron_list*)(t_fld))->items[1]));
ctron_list_push((ctron_list*)(t_su), (char*)(t_v_box((t_fv).v)));
t_k += 1;
}
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_su)}, t_out2);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Index")) == 0)) {
{
t_Flow t_ob = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)((t_ob).kind), (const char*)("k")) != 0)) {
{
return t_ob;
}
}
t_Flow t_ix = t_eval_expr(t_file, (t_ob).env, (t_ob).out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
if ((strcmp((const char*)((t_ix).kind), (const char*)("k")) != 0)) {
{
return t_ix;
}
}
t_Val t_arrv = (t_ob).v;
int32_t t_idx = (int32_t)(((t_ix).v).iv);
if (((t_arrv).tag != 8)) {
{
ctron_panic("index target");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((!t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_arrv)))->items[0]), "A"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_arrv)))->items[0]), "L")))) {
{
ctron_panic("index target");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((t_idx < 0)) {
{
ctron_panic("index target");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((t_idx >= (((ctron_list*)(t_cx_of(t_arrv)))->n - 1))) {
{
ctron_panic("index target");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("k", (t_ix).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_arrv)))->items[(t_idx + 1)]))), (t_ix).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Match")) == 0)) {
{
t_Flow t_sr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)((t_sr).kind), (const char*)("k")) != 0)) {
{
return t_sr;
}
}
const char* t_arms = ((const char*)((ctron_list*)(t_e))->items[2]);
int32_t t_i2 = 1;
int t_found = 0;
t_Flow t_res = t_e4("k", (t_sr).env, t_vV2(), (t_sr).out);
{
const char* t_arm = 0;
int t_hit = 0;
while (((t_i2 < ctron_len((const void*)(t_arms))) && (!t_found))) {
t_arm = ((const char*)((ctron_list*)(t_arms))->items[t_i2]);
ctron_list* t_pm = (ctron_list*)(t_pat_match((ctron_list*)(((const char*)((ctron_list*)(t_arm))->items[1])), (t_sr).v, (t_sr).env));
t_hit = t_seq2(((const char*)((ctron_list*)(t_pm))->items[0]), "1");
if ((t_hit && (ctron_len((const void*)(t_arm)) > 3))) {
{
t_Flow t_gr = t_eval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_pm))->items[1])), (t_sr).out, (ctron_list*)(((const char*)((ctron_list*)(t_arm))->items[3])));
if ((strcmp((const char*)((t_gr).kind), (const char*)("k")) != 0)) {
{
return t_gr;
}
}
t_hit = t_truth((t_gr).v);
}
}
if (t_hit) {
{
t_Flow t_ae = t_eval_expr(t_file, (ctron_list*)(((const char*)((ctron_list*)(t_pm))->items[1])), (t_sr).out, (ctron_list*)(((const char*)((ctron_list*)(t_arm))->items[2])));
t_res = t_ae;
t_found = 1;
}
}
t_i2 += 1;
}
}
if (t_found) {
{
return t_res;
}
}
ctron_panic("match no arm");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("TypeArgs")) == 0)) {
{
if (((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])))->items[1]), "Simd"))) {
{
ctron_list* t_sv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_sv), "SIMD");
ctron_list_push((ctron_list*)(t_sv), t_ty_head((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[1]))));
ctron_list_push((ctron_list*)(t_sv), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])))->items[2])))->items[1]));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_sv)}, t_out);
}
}
return t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
}
}
if ((strcmp((const char*)(t_t), (const char*)("Own")) == 0)) {
{
t_Flow t_obr = t_run_block(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), 0);
return t_e4((t_obr).kind, (t_obr).env, (t_obr).v, (t_obr).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Scope")) == 0)) {
{
ctron_list* t_scv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_scv), "SCOPE");
ctron_list* t_nenv = (ctron_list*)(t_env_add(t_env, ((const char*)((ctron_list*)(t_e))->items[1]), (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_scv)}));
t_Flow t_sbr = t_run_block(t_file, t_nenv, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])), 0);
return t_e4((t_sbr).kind, (t_sbr).env, (t_sbr).v, (t_sbr).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Try")) == 0)) {
{
t_Flow t_xr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[1])));
if ((strcmp((const char*)((t_xr).kind), (const char*)("k")) != 0)) {
{
return t_xr;
}
}
t_Val t_xv = (t_xr).v;
if (((t_xv).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[0]), "T")) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[1]), "None"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[1]), "Err"))) {
{
return t_e4("r", (t_xr).env, t_xv, (t_xr).out);
}
}
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[1]), "Some"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[1]), "Ok"))) {
{
if ((((ctron_list*)(t_cx_of(t_xv)))->n == 3)) {
{
return t_e4("k", (t_xr).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_xv)))->items[2]))), (t_xr).out);
}
}
return t_e4("k", (t_xr).env, t_xv, (t_xr).out);
}
}
}
}
}
}
return t_e4("k", (t_xr).env, t_xv, (t_xr).out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Call")) == 0)) {
{
return t_eval_call(t_file, t_env, t_out, t_e);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Void")) == 0)) {
{
return t_e4("k", t_env, t_vV2(), t_out);
}
}
if ((strcmp((const char*)(t_t), (const char*)("NParg")) == 0)) {
{
return t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_e))->items[2])));
}
}
ctron_panic(ctron_str_concat((const char*)("expr:"), (const char*)(t_t)));
return t_e4("a", t_env, t_vV2(), t_out);
}
const char* t_recv_cname(t_Val t_v) 
{
if (((t_v).tag == 8)) {
{
return ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]);
}
}
return "scalar";
}
t_Flow t_call_decl_vals(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_d, ctron_list* t_vals) 
{
const char* t_ps = ((const char*)((ctron_list*)(t_d))->items[3]);
ctron_list* t_nenv = (ctron_list*)(ctron_list_new());
int32_t t_k = 1;
{
const char* t_pr = 0;
while ((t_k < ctron_len((const void*)(t_ps)))) {
t_pr = ((const char*)((ctron_list*)(t_ps))->items[t_k]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pr))->items[0])), (const char*)("Param")) == 0)) {
{
t_nenv = (ctron_list*)(t_env_add(t_nenv, ((const char*)((ctron_list*)(t_pr))->items[2]), t_bind_of(t_file, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[(t_k - 1)]))))));
}
}
t_k += 1;
}
}
t_Flow t_br = t_run_block(t_file, t_nenv, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[5])), 0);
const char* t_fl = "k";
if ((strcmp((const char*)((t_br).kind), (const char*)("a")) == 0)) {
{
t_fl = "a";
}
}
return t_e4(t_fl, t_env, (t_br).v, (t_br).out);
}
t_Flow t_call_cv(ctron_list* t_file, ctron_list* t_env, const char* t_out, t_Val t_v, ctron_list* t_vals) 
{
if (((t_v).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "F")) {
{
ctron_list* t_d = (ctron_list*)(t_find_decl(t_file, ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1])));
if ((((ctron_list*)(t_d))->n == 0)) {
{
ctron_panic(ctron_str_concat((const char*)("no fn:"), (const char*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]))));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_call_decl_vals(t_file, t_env, t_out, t_d, t_vals);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[0]), "C")) {
{
const char* t_cap = ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[3]);
const char* t_cp = ((const char*)((ctron_list*)(t_cx_of(t_v)))->items[1]);
const char* t_nenv = t_cap;
int32_t t_k = 1;
while ((t_k < ctron_len((const void*)(t_cp)))) {
t_nenv = t_env_add((ctron_list*)(t_nenv), ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cp))->items[t_k])))->items[2]), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[(t_k - 1)]))));
t_k += 1;
}
t_Flow t_br = t_eval_expr(t_file, (ctron_list*)(t_nenv), t_out, (ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_v)))->items[2])));
return t_e4((t_br).kind, t_env, (t_br).v, (t_br).out);
}
}
}
}
ctron_panic("call value");
return t_e4("a", t_env, t_vV2(), t_out);
}
t_Flow t_call_id(ctron_list* t_file, ctron_list* t_env, const char* t_out, const char* t_nm, ctron_list* t_vals) 
{
if (t_seq2(t_nm, "StringBuilder")) {
{
if ((((ctron_list*)(t_vals))->n == 0)) {
{
ctron_list* t_sbv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_sbv), "SB");
return t_e4("k", t_env, t_cx_val(t_sbv), t_out);
}
}
ctron_panic("StringBuilder arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "print")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
return t_e4("k", t_env, t_vV2(), ctron_str_concat((const char*)(t_out), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))))));
}
}
ctron_panic("print arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "println")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
return t_e4("k", t_env, t_vV2(), ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))))), (const char*)("\n")));
}
}
ctron_panic("println arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "assert")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
if (t_truth(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))) {
{
return t_e4("k", t_env, t_vV2(), t_out);
}
}
return t_e4("a", t_env, t_vV2(), ctron_str_concat((const char*)(t_out), (const char*)("assert failed\n")));
}
}
ctron_panic("assert arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "assert_eq")) {
{
if ((((ctron_list*)(t_vals))->n == 2)) {
{
if (t_veq(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1]))))) {
{
return t_e4("k", t_env, t_vV2(), t_out);
}
}
return t_e4("a", t_env, t_vV2(), ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)("assert_eq failed: "))), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))))), (const char*)(" != "))), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1]))))))), (const char*)("\n")));
}
}
ctron_panic("assert_eq arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "assert_ne")) {
{
if ((((ctron_list*)(t_vals))->n == 2)) {
{
if ((!t_veq(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])))))) {
{
return t_e4("k", t_env, t_vV2(), t_out);
}
}
return t_e4("a", t_env, t_vS(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("assert_ne failed: "), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))))), (const char*)(" != "))), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1]))))))), t_out);
}
}
ctron_panic("assert_ne arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "read_file")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r0 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r0).tag == 5)) {
{
const char* t_rr = ctron_read_file((t_r0).s);
const char* t_mv = (const char*)(t_rr);
if (t_mv != NULL) {
const char* t_s = t_mv;
{
return t_e4("k", t_env, t_vS(t_s), t_out);
}
}
else {
{
return t_e4("k", t_env, t_vS(""), t_out);
}
}
}
}
}
}
ctron_panic("read_file args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "read_dir")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r1 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r1).tag == 5)) {
{
const char* t_rd = ctron_read_dir((t_r1).s);
const char* t_mv = (const char*)(t_rd);
if (t_mv != NULL) {
const char* t_s2 = t_mv;
{
ctron_list* t_tv2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv2), "T");
ctron_list_push((ctron_list*)(t_tv2), "Some");
ctron_list_push((ctron_list*)(t_tv2), (char*)(t_v_box(t_vS(t_s2))));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv2)}, t_out);
}
}
else {
{
ctron_list* t_tw2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tw2), "T");
ctron_list_push((ctron_list*)(t_tw2), "None");
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tw2)}, t_out);
}
}
}
}
}
}
ctron_panic("read_dir args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "fs_exists")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r2 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r2).tag == 5)) {
{
if ((ctron_fs_exists((t_r2).s)?1:0)) {
{
return t_e4("k", t_env, t_vB(1), t_out);
}
}
return t_e4("k", t_env, t_vB(0), t_out);
}
}
}
}
ctron_panic("fs_exists args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "fs_write")) {
{
if ((((ctron_list*)(t_vals))->n == 2)) {
{
t_Val t_r3 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
t_Val t_r4 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])));
if ((((t_r3).tag == 5) && ((t_r4).tag == 5))) {
{
if ((ctron_fs_write((t_r3).s, (t_r4).s)?1:0)) {
{
return t_e4("k", t_env, t_vB(1), t_out);
}
}
return t_e4("k", t_env, t_vB(0), t_out);
}
}
}
}
ctron_panic("fs_write args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "fs_delete")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r5 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r5).tag == 5)) {
{
if ((ctron_fs_delete((t_r5).s)?1:0)) {
{
return t_e4("k", t_env, t_vB(1), t_out);
}
}
return t_e4("k", t_env, t_vB(0), t_out);
}
}
}
}
ctron_panic("fs_delete args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "now_ms")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("now_ms arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("k", t_env, t_v6(ctron_now_ms_text()), t_out);
}
}
if (t_seq2(t_nm, "ctron_entry")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("ctron_entry arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("k", t_env, t_vS(ctron_entry()), t_out);
}
}
if (t_seq2(t_nm, "ctron_embedded")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("ctron_embedded arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("k", t_env, t_vS(t_gui_blocks_src(t_file)), t_out);
}
}
if (t_seq2(t_nm, "gui_sk_load")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("gui_sk_load arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
ctron_list* t_ntag = (ctron_list*)(ctron_list_new());
ctron_list* t_nflag = (ctron_list*)(ctron_list_new());
ctron_list* t_ncls = (ctron_list*)(ctron_list_new());
ctron_list* t_npre = (ctron_list*)(ctron_list_new());
ctron_list* t_nbid = (ctron_list*)(ctron_list_new());
ctron_list* t_npost = (ctron_list*)(ctron_list_new());
ctron_list* t_nfc = (ctron_list*)(ctron_list_new());
ctron_list* t_ns = (ctron_list*)(ctron_list_new());
ctron_list* t_nes = (ctron_list*)(ctron_list_new());
ctron_list* t_nec = (ctron_list*)(ctron_list_new());
ctron_list* t_ev_name = (ctron_list*)(ctron_list_new());
ctron_list* t_ev_fn = (ctron_list*)(ctron_list_new());
ctron_list* t_btns = (ctron_list*)(ctron_list_new());
ctron_list* t_sk = (ctron_list*)(ctron_list_new());
ctron_list* t_sv = (ctron_list*)(ctron_list_new());
ctron_list* t_sk_props = (ctron_list*)(ctron_list_new());
t_gui_sk_build(t_file, t_ntag, t_nflag, t_ncls, t_npre, t_nbid, t_npost, t_nfc, t_ns, t_nes, t_nec, t_ev_name, t_ev_fn, t_btns, t_sk, t_sv, t_sk_props);
ctron_list* t_u = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_u), "U");
ctron_list_push((ctron_list*)(t_u), "GuiTree");
ctron_list_push((ctron_list*)(t_u), "ntag");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_ntag)));
ctron_list_push((ctron_list*)(t_u), "nflag");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_nflag)));
ctron_list_push((ctron_list*)(t_u), "ncls");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_ncls)));
ctron_list_push((ctron_list*)(t_u), "npre");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_npre)));
ctron_list_push((ctron_list*)(t_u), "nbid");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_nbid)));
ctron_list_push((ctron_list*)(t_u), "npost");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_npost)));
ctron_list_push((ctron_list*)(t_u), "nfc");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_i(t_nfc)));
ctron_list_push((ctron_list*)(t_u), "ns");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_i(t_ns)));
ctron_list_push((ctron_list*)(t_u), "nes");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_i(t_nes)));
ctron_list_push((ctron_list*)(t_u), "nec");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_i(t_nec)));
ctron_list_push((ctron_list*)(t_u), "ev_name");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_ev_name)));
ctron_list_push((ctron_list*)(t_u), "ev_fn");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_ev_fn)));
ctron_list_push((ctron_list*)(t_u), "btns");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_i(t_btns)));
ctron_list_push((ctron_list*)(t_u), "sk");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_sk)));
ctron_list_push((ctron_list*)(t_u), "sv");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_sv)));
ctron_list_push((ctron_list*)(t_u), "props");
ctron_list_push((ctron_list*)(t_u), (char*)(t_gui_sk_wrap_s(t_sk_props)));
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_u)}, t_out);
}
}
if (t_seq2(t_nm, "ctron_exe_path")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("ctron_exe_path arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("k", t_env, t_vS(ctron_exe_path()), t_out);
}
}
if (t_seq2(t_nm, "env_get")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r6 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r6).tag == 5)) {
{
return t_e4("k", t_env, t_vS(ctron_env_get((t_r6).s)), t_out);
}
}
}
}
ctron_panic("env_get args");
return t_e4("k", t_env, t_vS(""), t_out);
}
}
if (t_seq2(t_nm, "ctron_cli_flag")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r7 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r7).tag == 5)) {
{
return t_e4("k", t_env, t_vS(ctron_cli_flag((t_r7).s)), t_out);
}
}
}
}
ctron_panic("ctron_cli_flag args");
return t_e4("k", t_env, t_vS(""), t_out);
}
}
if (t_seq2(t_nm, "utf8_enc")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r8 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r8).tag == 1)) {
{
return t_e4("k", t_env, t_vS(ctron_utf8_enc((int)(t_dvi_str((int32_t)((t_r8).iv))))), t_out);
}
}
}
}
ctron_panic("utf8_enc arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "byte_at")) {
{
if ((((ctron_list*)(t_vals))->n == 2)) {
{
t_Val t_r9 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
t_Val t_r10 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])));
if ((((t_r9).tag == 5) && ((t_r10).tag == 1))) {
{
return t_e4("k", t_env, t_vI(ctron_byte_at((t_r9).s, (int32_t)((t_r10).iv))), t_out);
}
}
}
}
ctron_panic("byte_at arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "byte_slice")) {
{
if ((((ctron_list*)(t_vals))->n == 3)) {
{
t_Val t_r11 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
t_Val t_r12 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])));
t_Val t_r13 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[2])));
if (((((t_r11).tag == 5) && ((t_r12).tag == 1)) && ((t_r13).tag == 1))) {
{
return t_e4("k", t_env, t_vS(ctron_byte_slice((t_r11).s, (int32_t)((t_r12).iv), (int32_t)((t_r13).iv))), t_out);
}
}
}
}
ctron_panic("byte_slice arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "str_from_c")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_r14 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_r14).tag == 5)) {
{
return t_e4("k", t_env, t_vS((t_r14).s), t_out);
}
}
}
}
ctron_panic("str_from_c arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "errno")) {
{
if ((((ctron_list*)(t_vals))->n != 0)) {
{
ctron_panic("errno arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_e4("i", t_env, t_vI((int32_t)errno), t_out);
}
}
if (t_or2(t_seq2(t_nm, "clo_handle"), t_or2(t_seq2(t_nm, "clo_cb2"), t_seq2(t_nm, "clo_cb3")))) {
{
ctron_panic("clo_handle/clo_cb 仅原生口径可用(经 ctron-emit + cc 链接后运行)");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_or2(t_seq2(t_nm, "dlopen"), t_or2(t_seq2(t_nm, "dlclose"), t_seq2(t_nm, "dlsym")))) {
{
ctron_panic("dlopen/dlsym 仅原生口径可用(经 ctron-emit + cc 链接后运行)");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_seq2(t_nm, "panic")) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
return t_e4("a", t_env, t_vV2(), ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_out), (const char*)(t_fmt(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))))), (const char*)("\n")));
}
}
ctron_panic("panic arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (t_or2(t_seq2(t_nm, "Some"), t_or2(t_seq2(t_nm, "None"), t_or2(t_seq2(t_nm, "Ok"), t_seq2(t_nm, "Err"))))) {
{
ctron_list* t_tv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv), "T");
ctron_list_push((ctron_list*)(t_tv), t_nm);
int32_t t_qq = 0;
while ((t_qq < ((ctron_list*)(t_vals))->n)) {
ctron_list_push((ctron_list*)(t_tv), ((const char*)((ctron_list*)(t_vals))->items[t_qq]));
t_qq += 1;
}
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv)}, t_out);
}
}
ctron_list* t_evb = (ctron_list*)(t_env_at(t_env, t_nm));
if ((((ctron_list*)(t_evb))->n > 0)) {
{
t_Val t_kv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_evb))->items[1])));
if (((t_kv).tag == 8)) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_kv)))->items[0]), "C"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_kv)))->items[0]), "F"))) {
{
return t_call_cv(t_file, t_env, t_out, t_kv, t_vals);
}
}
}
}
}
}
ctron_list* t_d = (ctron_list*)(t_find_decl(t_file, t_nm));
if ((((ctron_list*)(t_d))->n == 0)) {
{
int32_t t_va = t_variant_arity(t_file, t_nm);
if ((t_va >= 0)) {
{
if ((t_va == ((ctron_list*)(t_vals))->n)) {
{
ctron_list* t_tv3 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv3), "T");
ctron_list_push((ctron_list*)(t_tv3), t_nm);
int32_t t_q3 = 0;
while ((t_q3 < ((ctron_list*)(t_vals))->n)) {
ctron_list_push((ctron_list*)(t_tv3), ((const char*)((ctron_list*)(t_vals))->items[t_q3]));
t_q3 += 1;
}
return t_e4("k", t_env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv3)}, t_out);
}
}
ctron_panic(ctron_str_concat((const char*)("variant arity:"), (const char*)(t_nm)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((((ctron_list*)(t_decl_node(t_file, "FnExt", t_nm)))->n > 0)) {
{
const char* t_f0 = "";
const char* t_f1 = "";
const char* t_f2 = "";
const char* t_f3 = "";
const char* t_f4 = "";
const char* t_f5 = "";
const char* t_f6 = "";
const char* t_f7 = "";
const char* t_f8 = "";
const char* t_f9 = "";
const char* t_f10 = "";
const char* t_f11 = "";
int32_t t_q = 0;
{
const char* t_pl = 0;
while ((t_q < ((ctron_list*)(t_vals))->n)) {
t_Val t_xv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[t_q])));
t_pl = "";
if (((t_xv).tag == 5)) {
{
t_pl = ctron_str_concat((const char*)("s:"), (const char*)((t_xv).s));
}
}
else {
{
if (((t_xv).tag == 4)) {
{
if ((t_xv).b) {
{
t_pl = "i:1";
}
}
else {
{
t_pl = "i:0";
}
}
}
}
else {
{
if (((t_xv).tag == 1)) {
{
t_pl = ctron_str_concat((const char*)("i:"), (const char*)(t_dvi_str((int32_t)((t_xv).iv))));
}
}
else {
{
ctron_panic(ctron_str_concat((const char*)("extern 解释口径:不支持参数类型 "), (const char*)(ctron_i32_to_string((int32_t)((t_xv).tag)))));
}
}
}
}
}
}
if ((t_q == 0)) {
{
t_f0 = t_pl;
}
}
if ((t_q == 1)) {
{
t_f1 = t_pl;
}
}
if ((t_q == 2)) {
{
t_f2 = t_pl;
}
}
if ((t_q == 3)) {
{
t_f3 = t_pl;
}
}
if ((t_q == 4)) {
{
t_f4 = t_pl;
}
}
if ((t_q == 5)) {
{
t_f5 = t_pl;
}
}
if ((t_q == 6)) {
{
t_f6 = t_pl;
}
}
if ((t_q == 7)) {
{
t_f7 = t_pl;
}
}
if ((t_q == 8)) {
{
t_f8 = t_pl;
}
}
if ((t_q == 9)) {
{
t_f9 = t_pl;
}
}
if ((t_q == 10)) {
{
t_f10 = t_pl;
}
}
if ((t_q == 11)) {
{
t_f11 = t_pl;
}
}
if ((t_q > 11)) {
{
ctron_panic("extern 解释口径:参数超 12");
}
}
t_q += 1;
}
}
int32_t t_rr = ctron_ext_dispatch(t_nm, t_f0, t_f1, t_f2, t_f3, t_f4, t_f5, t_f6, t_f7, t_f8, t_f9, t_f10, t_f11);
return t_e4("k", t_env, t_vI(t_rr), t_out);
}
}
ctron_panic(ctron_str_concat((const char*)("no fn:"), (const char*)(t_nm)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
return t_call_decl_vals(t_file, t_env, t_out, t_d, t_vals);
}
int32_t t_variant_arity(ctron_list* t_file, const char* t_nm) 
{
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Enum")) == 0)) {
{
const char* t_vs = ((const char*)((ctron_list*)(t_d))->items[3]);
int32_t t_j = 1;
{
const char* t_vt = 0;
while ((t_j < ctron_len((const void*)(t_vs)))) {
t_vt = ((const char*)((ctron_list*)(t_vs))->items[t_j]);
if (t_seq2(((const char*)((ctron_list*)(t_vt))->items[1]), t_nm)) {
{
const char* t_kd = ((const char*)((ctron_list*)(t_vt))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_kd))->items[0])), (const char*)("KTuple")) == 0)) {
{
return (ctron_len((const void*)(t_kd)) - 1);
}
}
return 0;
}
}
t_j += 1;
}
}
}
}
t_i += 1;
}
}
return (0 - 1);
}
t_Val t_vT(const char* t_vt, t_Val t_p) 
{
ctron_list* t_v = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_v), "T");
ctron_list_push((ctron_list*)(t_v), t_vt);
ctron_list_push((ctron_list*)(t_v), (char*)(t_v_box(t_p)));
return (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_v)};
}
t_Val t_conv_as_6(t_Val t_v, const char* t_tgt) 
{
const char* t_mag = t_c6can(t_v6t(t_v));
int t_neg = 0;
if ((strlen((const char*)(t_mag)) > 0)) {
{
if ((ctron_byte_at(t_mag, 0) == 45)) {
{
t_neg = 1;
t_mag = ctron_byte_slice(t_mag, 1, strlen((const char*)(t_mag)));
}
}
}
}
const char* t_modv = "";
const char* t_smax = "";
const char* t_wl = "";
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U8")) == 0), (strcmp((const char*)(t_tgt), (const char*)("I8")) == 0))) {
{
t_modv = "256";
t_wl = "u8";
if ((strcmp((const char*)(t_tgt), (const char*)("I8")) == 0)) {
{
t_wl = "i8";
t_smax = "127";
}
}
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U16")) == 0), (strcmp((const char*)(t_tgt), (const char*)("I16")) == 0))) {
{
t_modv = "65536";
t_wl = "u16";
if ((strcmp((const char*)(t_tgt), (const char*)("I16")) == 0)) {
{
t_wl = "i16";
t_smax = "32767";
}
}
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U32")) == 0), (strcmp((const char*)(t_tgt), (const char*)("I32")) == 0))) {
{
t_modv = "4294967296";
t_wl = "i";
if ((strcmp((const char*)(t_tgt), (const char*)("I32")) == 0)) {
{
t_smax = "2147483647";
}
}
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("I64")) == 0), t_or2((strcmp((const char*)(t_tgt), (const char*)("U64")) == 0), t_or2((strcmp((const char*)(t_tgt), (const char*)("ISize")) == 0), (strcmp((const char*)(t_tgt), (const char*)("USize")) == 0))))) {
{
t_modv = "18446744073709551616";
t_wl = "6";
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("I64")) == 0), (strcmp((const char*)(t_tgt), (const char*)("ISize")) == 0))) {
{
t_smax = "9223372036854775807";
}
}
}
}
if ((strlen((const char*)(t_modv)) == 0)) {
{
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("F64")) == 0), (strcmp((const char*)(t_tgt), (const char*)("F32")) == 0))) {
{
return t_vD(t_df_can(t_v6t(t_v)));
}
}
return t_v;
}
}
const char* t_qr = t_c6divmod(t_mag, t_modv);
int32_t t_bar = 0;
int32_t t_ci = 0;
while ((t_ci < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_ci) == 124)) {
{
t_bar = t_ci;
}
}
t_ci += 1;
}
const char* t_m = ctron_byte_slice(t_qr, (t_bar + 1), strlen((const char*)(t_qr)));
if (t_neg) {
{
if ((strcmp((const char*)(t_m), (const char*)("0")) != 0)) {
{
t_m = t_c6sub(t_modv, t_m);
}
}
}
}
if ((strlen((const char*)(t_smax)) > 0)) {
{
if ((t_c6abscmp(t_m, t_smax) > 0)) {
{
t_m = t_c6sub(t_m, t_modv);
}
}
}
}
if ((strcmp((const char*)(t_wl), (const char*)("i")) == 0)) {
{
return t_vI(t_dvi(t_m));
}
}
if ((strcmp((const char*)(t_wl), (const char*)("6")) == 0)) {
{
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U64")) == 0), (strcmp((const char*)(t_tgt), (const char*)("USize")) == 0))) {
{
return t_v7(t_c6can(t_m));
}
}
return t_v6(t_c6can(t_m));
}
}
return t_vW(t_wl, t_dvi(t_m));
}
t_Val t_conv_as(t_Val t_v, const char* t_tgt) 
{
if (t_or2(((t_v).tag == 2), ((t_v).tag == 3))) {
{
return t_conv_as_6(t_v, t_tgt);
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U64")) == 0), (strcmp((const char*)(t_tgt), (const char*)("USize")) == 0))) {
{
return t_conv_as_6(t_v, t_tgt);
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("F64")) == 0), (strcmp((const char*)(t_tgt), (const char*)("F32")) == 0))) {
{
if (t_or2(((t_v).tag == 1), ((t_v).tag == 7))) {
{
return t_vD(t_df_can(t_dvi_str((int32_t)((t_v).iv))));
}
}
return t_v;
}
}
if (t_or2((strcmp((const char*)(t_tgt), (const char*)("U8")) == 0), t_or2((strcmp((const char*)(t_tgt), (const char*)("I8")) == 0), t_or2((strcmp((const char*)(t_tgt), (const char*)("U16")) == 0), (strcmp((const char*)(t_tgt), (const char*)("I16")) == 0))))) {
{
const char* t_wl = "u8";
if ((strcmp((const char*)(t_tgt), (const char*)("I8")) == 0)) {
{
t_wl = "i8";
}
}
if ((strcmp((const char*)(t_tgt), (const char*)("U16")) == 0)) {
{
t_wl = "u16";
}
}
if ((strcmp((const char*)(t_tgt), (const char*)("I16")) == 0)) {
{
t_wl = "i16";
}
}
int32_t t_iv = 0;
if (((t_v).tag == 6)) {
{
t_iv = t_d_intpart((t_v).s);
}
}
else {
{
t_iv = (int32_t)((t_v).iv);
}
}
int32_t t_s = 1;
int32_t t_k = 0;
while ((t_k < t_w_bits(t_wl))) {
t_s = (t_s * 2);
t_k += 1;
}
int32_t t_m = (t_iv % t_s);
if ((t_m < 0)) {
{
t_m += t_s;
}
}
if ((t_w_signed(t_wl) && (t_m > t_w_max(t_wl)))) {
{
t_m -= t_s;
}
}
return t_vW(t_wl, t_m);
}
}
if (((t_v).tag == 6)) {
{
return t_vI(t_d_intpart((t_v).s));
}
}
if (((t_v).tag == 7)) {
{
return t_vI((int32_t)((t_v).iv));
}
}
return t_v;
}
int32_t t_d_intpart(const char* t_t) 
{
int32_t t_i = 0;
int32_t t_n = strlen((const char*)(t_t));
while ((t_i < t_n)) {
if ((ctron_byte_at(t_t, t_i) == 46)) {
{
t_n = t_i;
}
}
t_i += 1;
}
return t_dvi(ctron_byte_slice(t_t, 0, t_n));
}
t_Flow t_ns_map_reduce(ctron_list* t_file, t_Flow t_ob, ctron_list* t_vals, int t_ismap) 
{
t_Val t_av = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if ((!((t_av).tag == 8))) {
{
ctron_panic("parallel target");
return t_e4("a", (t_ob).env, t_vV2(), (t_ob).out);
}
}
if ((!t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_av)))->items[0]), "A"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_av)))->items[0]), "L")))) {
{
ctron_panic("parallel target");
return t_e4("a", (t_ob).env, t_vV2(), (t_ob).out);
}
}
if (t_ismap) {
{
ctron_list* t_outv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_outv), ((const char*)((ctron_list*)(t_cx_of(t_av)))->items[0]));
int32_t t_i = 1;
while ((t_i < ((ctron_list*)(t_cx_of(t_av)))->n)) {
ctron_list* t_fa = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_fa), ((const char*)((ctron_list*)(t_cx_of(t_av)))->items[t_i]));
t_Flow t_er = t_call_cv(t_file, (t_ob).env, (t_ob).out, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1]))), t_fa);
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
ctron_list_push((ctron_list*)(t_outv), (char*)(t_v_box((t_er).v)));
t_i += 1;
}
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_outv)}, (t_ob).out);
}
}
t_Val t_acc = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])));
int32_t t_i2 = 1;
while ((t_i2 < ((ctron_list*)(t_cx_of(t_av)))->n)) {
ctron_list* t_fa2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_fa2), (char*)(t_v_box(t_acc)));
ctron_list_push((ctron_list*)(t_fa2), ((const char*)((ctron_list*)(t_cx_of(t_av)))->items[t_i2]));
t_Flow t_er2 = t_call_cv(t_file, (t_ob).env, (t_ob).out, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[2]))), t_fa2);
if ((strcmp((const char*)((t_er2).kind), (const char*)("k")) != 0)) {
{
return t_er2;
}
}
t_acc = (t_er2).v;
t_i2 += 1;
}
return t_e4("k", (t_ob).env, t_acc, (t_ob).out);
}
int t_utf8_cont(const char* t_s, int32_t t_k) 
{
int32_t t_c = ctron_byte_at(t_s, t_k);
return ((t_c >= 128) && (t_c < 192));
}
int t_str_contains(const char* t_s, const char* t_sub) 
{
int32_t t_n = strlen((const char*)(t_s));
int32_t t_m = strlen((const char*)(t_sub));
if ((t_m == 0)) {
{
return 1;
}
}
int32_t t_i = 0;
{
int t_ok = 0;
while (((t_i + t_m) <= t_n)) {
int32_t t_j = 0;
t_ok = 1;
while (((t_j < t_m) && t_ok)) {
if ((ctron_byte_at(t_s, (t_i + t_j)) != ctron_byte_at(t_sub, t_j))) {
{
t_ok = 0;
}
}
t_j += 1;
}
if (t_ok) {
{
return 1;
}
}
t_i += 1;
}
}
return 0;
}
t_Flow t_call_mem(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_cal, ctron_list* t_vals) 
{
t_Flow t_ob = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[1])));
if ((strcmp((const char*)((t_ob).kind), (const char*)("k")) != 0)) {
{
return t_ob;
}
}
t_Val t_recv = (t_ob).v;
const char* t_m = ((const char*)((ctron_list*)(t_cal))->items[3]);
if (((t_recv).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "SB")) {
{
if ((t_seq2(t_m, "push_str") && (((ctron_list*)(t_vals))->n == 1))) {
{
t_Val t_sv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_sv).tag != 5)) {
{
ctron_panic("push_str 需 Str");
}
}
ctron_list_push((ctron_list*)(t_cx_of(t_recv)), (t_sv).s);
return t_e4("k", (t_ob).env, t_vV2(), (t_ob).out);
}
}
if ((t_seq2(t_m, "to_string") && (((ctron_list*)(t_vals))->n == 0))) {
{
const char* t_acc = "";
int32_t t_si = 1;
while ((t_si < ((ctron_list*)(t_cx_of(t_recv)))->n)) {
t_acc = ctron_str_concat((const char*)(t_acc), (const char*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[t_si])));
t_si += 1;
}
return t_e4("k", (t_ob).env, t_vS(t_acc), (t_ob).out);
}
}
ctron_panic(ctron_str_concat((const char*)("StringBuilder 方法:"), (const char*)(t_m)));
return t_e4("a", (t_ob).env, t_vV2(), (t_ob).out);
}
}
}
}
if (t_seq2(t_m, "to_string")) {
{
if ((((ctron_list*)(t_vals))->n == 0)) {
{
return t_e4("k", (t_ob).env, t_vS(t_fmt(t_recv)), (t_ob).out);
}
}
ctron_panic("to_string arity");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (((t_recv).tag == 8)) {
{
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "SCOPE") && t_seq2(t_m, "spawn")) && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list* t_noargs = (ctron_list*)(ctron_list_new());
t_Flow t_sr = t_call_cv(t_file, (t_ob).env, (t_ob).out, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), t_noargs);
ctron_list* t_tk = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tk), "TASK");
if ((strcmp((const char*)((t_sr).kind), (const char*)("a")) == 0)) {
{
ctron_list_push((ctron_list*)(t_tk), "P");
ctron_list_push((ctron_list*)(t_tk), (char*)(t_v_box((t_sr).v)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_tk), "K");
ctron_list_push((ctron_list*)(t_tk), (char*)(t_v_box((t_sr).v)));
}
}
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tk)}, (t_sr).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "TASK") && t_seq2(t_m, "join")) && (((ctron_list*)(t_vals))->n == 0))) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "P")) {
{
return t_e4("a", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]))), (t_ob).out);
}
}
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]))), (t_ob).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "TASK") && t_seq2(t_m, "join_or")) && (((ctron_list*)(t_vals))->n == 0))) {
{
ctron_list* t_jr = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_jr), "T");
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "P")) {
{
ctron_list_push((ctron_list*)(t_jr), "Err");
ctron_list_push((ctron_list*)(t_jr), (char*)(t_v_box(t_vS("task panic"))));
}
}
else {
{
ctron_list_push((ctron_list*)(t_jr), "Ok");
ctron_list_push((ctron_list*)(t_jr), ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]));
}
}
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_jr)}, (t_ob).out);
}
}
if (((t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "MUX"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "M")) && t_or2(t_seq2(t_m, "with_mut"), t_seq2(t_m, "with"))) && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list* t_fa2 = (ctron_list*)(ctron_list_new());
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "M") && t_seq2(t_m, "with_mut"))) {
{
ctron_list_push((ctron_list*)(t_fa2), (char*)(t_v_box(t_recv)));
}
}
else {
{
ctron_list_push((ctron_list*)(t_fa2), ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]));
}
}
return t_call_cv(t_file, (t_ob).env, (t_ob).out, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), t_fa2);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "M") && t_seq2(t_m, "fetch_add")) && (((ctron_list*)(t_vals))->n == 1))) {
{
t_Val t_oldm = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1])));
ctron_list* t_rcx = (ctron_list*)(t_cx_of(t_recv));
((ctron_list*)(t_rcx))->items[1] = (char*)(t_v_box(t_val_arith("Add", t_oldm, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))));
return t_e4("k", (t_ob).env, t_oldm, (t_ob).out);
}
}
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "NS") && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "dom"))) {
{
if ((t_seq2(t_m, "set_title") && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list* t_dk = (ctron_list*)(t_env_at(t_env, "#dom:title"));
ctron_list* t_denv = (ctron_list*)(t_env);
if ((((ctron_list*)(t_dk))->n > 0)) {
{
t_denv = (ctron_list*)(t_env_set(t_env, "#dom:title", t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))));
}
}
else {
{
t_denv = (ctron_list*)(t_env_add(t_env, "#dom:title", t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))));
}
}
return t_e4("k", t_denv, t_vV2(), t_out);
}
}
if ((t_seq2(t_m, "title") && (((ctron_list*)(t_vals))->n == 0))) {
{
ctron_list* t_te = (ctron_list*)(t_env_at(t_env, "#dom:title"));
t_Val t_tv = t_vS("");
if ((((ctron_list*)(t_te))->n > 1)) {
{
t_tv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_te))->items[1])));
}
}
return t_e4("k", t_env, t_tv, t_out);
}
}
ctron_panic(ctron_str_concat((const char*)("dom 方法:"), (const char*)(t_m)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "NS") && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "bit"))) {
{
ctron_list* t_obold = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_obold), (t_ob).kind);
ctron_list_push((ctron_list*)(t_obold), (char*)((t_ob).env));
ctron_list_push((ctron_list*)(t_obold), (char*)(t_v_box((t_ob).v)));
ctron_list_push((ctron_list*)(t_obold), (t_ob).out);
return t_ns_bit(t_m, t_vals, t_obold);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "NS") && t_seq2(t_m, "map")) && (((ctron_list*)(t_vals))->n == 2))) {
{
return t_ns_map_reduce(t_file, t_ob, t_vals, 1);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "NS") && t_seq2(t_m, "reduce")) && (((ctron_list*)(t_vals))->n == 3))) {
{
return t_ns_map_reduce(t_file, t_ob, t_vals, 0);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "CH") && t_seq2(t_m, "send")) && (((ctron_list*)(t_vals))->n == 1))) {
{
int32_t t_capn = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[3])))).iv);
const char* t_chq = ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]);
if (((t_capn >= 0) && (ctron_len((const void*)(t_chq)) >= t_capn))) {
{
return t_e4("k", (t_ob).env, t_vT("Err", t_vS("ScopeCancelled")), (t_ob).out);
}
}
ctron_list_push((ctron_list*)(t_chq), (char*)(t_v_box(t_bind_of(t_file, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))))));
return t_e4("k", (t_ob).env, t_vT("Ok", t_vV2()), (t_ob).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "CH") && t_seq2(t_m, "recv")) && (((ctron_list*)(t_vals))->n == 0))) {
{
const char* t_cur = ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]);
const char* t_q2 = ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]);
int32_t t_idx = t_dvi(((const char*)((ctron_list*)(t_cur))->items[1]));
if ((t_idx >= ctron_len((const void*)(t_q2)))) {
{
return t_e4("k", (t_ob).env, t_vT("Err", t_vS("ScopeCancelled")), (t_ob).out);
}
}
((ctron_list*)(t_cur))->items[1] = ctron_i32_to_string((int32_t)((t_idx + 1)));
return t_e4("k", (t_ob).env, t_vT("Ok", t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_q2))->items[t_idx])))), (t_ob).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "SIMD") && t_seq2(t_m, "splat")) && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list* t_vc = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_vc), "VEC");
int32_t t_n = t_dvi(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]));
int32_t t_vi = 0;
while ((t_vi < t_n)) {
ctron_list_push((ctron_list*)(t_vc), (char*)(t_v_box(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))));
t_vi += 1;
}
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_vc)}, (t_ob).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "VEC") && t_seq2(t_m, "lane")) && (((ctron_list*)(t_vals))->n == 1))) {
{
int32_t t_li = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))).iv);
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[(t_li + 1)]))), (t_ob).out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "VEC") && t_seq2(t_m, "to_array")) && (((ctron_list*)(t_vals))->n == 0))) {
{
ctron_list* t_arr2 = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_arr2), "A");
int32_t t_ai = 1;
while ((t_ai < ((ctron_list*)(t_cx_of(t_recv)))->n)) {
ctron_list_push((ctron_list*)(t_arr2), ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[t_ai]));
t_ai += 1;
}
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_arr2)}, (t_ob).out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "T")) {
{
if ((t_seq2(t_m, "or") && (((ctron_list*)(t_vals))->n == 1))) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Some"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Ok"))) {
{
if ((((ctron_list*)(t_cx_of(t_recv)))->n == 3)) {
{
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]))), (t_ob).out);
}
}
return t_e4("k", (t_ob).env, t_recv, (t_ob).out);
}
}
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), (t_ob).out);
}
}
if ((t_seq2(t_m, "expect") && (((ctron_list*)(t_vals))->n == 1))) {
{
t_Val t_ev = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_ev).tag == 5)) {
{
if (t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Some"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Ok"))) {
{
if ((((ctron_list*)(t_cx_of(t_recv)))->n == 3)) {
{
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]))), (t_ob).out);
}
}
return t_e4("k", (t_ob).env, t_recv, (t_ob).out);
}
}
return t_e4("a", (t_ob).env, t_vS((t_ev).s), (t_ob).out);
}
}
}
}
if ((t_seq2(t_m, "is_some") && (((ctron_list*)(t_vals))->n == 0))) {
{
return t_e4("k", (t_ob).env, t_vB(t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Some"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Ok"))), (t_ob).out);
}
}
if ((t_seq2(t_m, "map") && (((ctron_list*)(t_vals))->n == 1))) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Some") && (((ctron_list*)(t_cx_of(t_recv)))->n == 3))) {
{
ctron_list* t_fa = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_fa), ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2]));
t_Flow t_mr = t_call_cv(t_file, (t_ob).env, (t_ob).out, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))), t_fa);
if ((strcmp((const char*)((t_mr).kind), (const char*)("k")) != 0)) {
{
return t_mr;
}
}
ctron_list* t_tv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tv), "T");
ctron_list_push((ctron_list*)(t_tv), "Some");
ctron_list_push((ctron_list*)(t_tv), (char*)(t_v_box((t_mr).v)));
return t_e4("k", (t_mr).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tv)}, (t_mr).out);
}
}
return t_e4("k", (t_ob).env, t_recv, (t_ob).out);
}
}
if ((t_seq2(t_m, "context") && (((ctron_list*)(t_vals))->n == 1))) {
{
t_Val t_ev2 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_ev2).tag == 5)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), "Err")) {
{
t_Val t_p = t_recv;
if ((((ctron_list*)(t_cx_of(t_recv)))->n == 3)) {
{
t_p = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[2])));
}
}
ctron_list* t_cs = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_cs), "T");
ctron_list_push((ctron_list*)(t_cs), "Some");
ctron_list_push((ctron_list*)(t_cs), (char*)(t_v_box(t_p)));
ctron_list* t_eb = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_eb), "U");
ctron_list_push((ctron_list*)(t_eb), "AnyError");
ctron_list_push((ctron_list*)(t_eb), "message");
ctron_list_push((ctron_list*)(t_eb), (char*)(t_v_box(t_vS((t_ev2).s))));
ctron_list_push((ctron_list*)(t_eb), "cause");
ctron_list_push((ctron_list*)(t_eb), (char*)(t_v_box((t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_cs)})));
ctron_list_push((ctron_list*)(t_eb), "trace");
ctron_list* t_trl = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_trl), "L");
ctron_list_push((ctron_list*)(t_trl), (char*)(t_v_box(t_vS(ctron_str_concat((const char*)("ctx:"), (const char*)((t_ev2).s))))));
ctron_list_push((ctron_list*)(t_eb), (char*)(t_v_box((t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_trl)})));
ctron_list* t_te = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_te), "T");
ctron_list_push((ctron_list*)(t_te), "Err");
ctron_list_push((ctron_list*)(t_te), (char*)(t_v_box((t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_eb)})));
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_te)}, (t_ob).out);
}
}
return t_e4("k", (t_ob).env, t_recv, (t_ob).out);
}
}
}
}
}
}
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "U") && t_seq2(t_m, "eq"))) {
{
if ((((ctron_list*)(t_vals))->n == 1)) {
{
t_Val t_ov2 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_ov2).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_ov2)))->items[0]), "U")) {
{
return t_e4("k", (t_ob).env, t_vB(t_eq_val(t_recv, t_ov2)), (t_ob).out);
}
}
}
}
}
}
ctron_panic("eq 需同型结构实参");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "U") && t_seq2(t_m, "show")) && (((ctron_list*)(t_vals))->n == 0))) {
{
return t_e4("k", (t_ob).env, t_vS(t_fmt(t_recv)), (t_ob).out);
}
}
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "U")) {
{
ctron_list* t_mth = (ctron_list*)(t_find_impl_method(t_file, ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]), t_m));
if ((((ctron_list*)(t_mth))->n > 0)) {
{
return t_call_method_vals(t_file, (t_ob).env, (t_ob).out, t_mth, t_cx_of(t_recv), t_vals);
}
}
if (t_is_class(t_file, ((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]))) {
{
ctron_panic(ctron_str_concat((const char*)("unknown method:"), (const char*)(t_m)));
return t_e4("a", t_env, t_vV2(), t_out);
}
}
}
}
}
}
if (t_seq2(t_m, "slice")) {
{
if (((((ctron_list*)(t_vals))->n == 1) && ((t_recv).tag == 5))) {
{
t_Val t_rv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if ((((t_rv).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_rv)))->items[0]), "R"))) {
{
int32_t t_lo = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_rv)))->items[2])))).iv);
int32_t t_hi = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_rv)))->items[3])))).iv);
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_rv)))->items[1]), "true")) {
{
t_hi += 1;
}
}
int32_t t_ln = strlen((const char*)((t_recv).s));
if ((((t_lo > 0) && (t_lo < t_ln)) && t_utf8_cont((t_recv).s, t_lo))) {
{
return t_e4("a", (t_ob).env, t_vS("invalid utf8 boundary"), (t_ob).out);
}
}
if ((((t_hi > 0) && (t_hi < t_ln)) && t_utf8_cont((t_recv).s, t_hi))) {
{
return t_e4("a", (t_ob).env, t_vS("invalid utf8 boundary"), (t_ob).out);
}
}
if ((t_lo < 0)) {
{
t_lo = 0;
}
}
if ((t_hi > t_ln)) {
{
t_hi = t_ln;
}
}
if ((t_hi < t_lo)) {
{
t_hi = t_lo;
}
}
return t_e4("k", (t_ob).env, t_vS(ctron_byte_slice((t_recv).s, t_lo, t_hi)), (t_ob).out);
}
}
}
}
ctron_panic("slice args");
return t_e4("a", t_env, t_vV2(), t_out);
}
}
if (((t_seq2(t_m, "contains") && ((t_recv).tag == 5)) && (((ctron_list*)(t_vals))->n == 1))) {
{
t_Val t_sv2 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
if (((t_sv2).tag != 5)) {
{
ctron_panic("contains 需 Str");
}
}
return t_e4("k", (t_ob).env, t_vB(t_str_contains((t_recv).s, (t_sv2).s)), (t_ob).out);
}
}
if (((t_recv).tag == 8)) {
{
if (t_seq2(t_m, "load")) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "M") && (((ctron_list*)(t_vals))->n == 0))) {
{
return t_e4("k", (t_ob).env, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[1]))), (t_ob).out);
}
}
}
}
if (t_seq2(t_m, "store")) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "M") && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list* t_srcx = (ctron_list*)(t_cx_of(t_recv));
((ctron_list*)(t_srcx))->items[1] = (char*)(t_v_box(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))));
return t_e4("k", (t_ob).env, t_vV2(), (t_ob).out);
}
}
}
}
if (t_seq2(t_m, "push")) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "L") && (((ctron_list*)(t_vals))->n == 1))) {
{
ctron_list_push((ctron_list*)(t_cx_of(t_recv)), (char*)(t_v_box(t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0]))))));
return t_e4("k", (t_ob).env, t_vV2(), (t_ob).out);
}
}
}
}
if (t_seq2(t_m, "into_gc")) {
{
if ((t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "L") && (((ctron_list*)(t_vals))->n == 0))) {
{
return t_e4("k", (t_ob).env, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_vdeep(t_cx_of(t_recv)))}, (t_ob).out);
}
}
}
}
if (((t_seq2(t_m, "contains") && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[0]), "L")) && (((ctron_list*)(t_vals))->n == 1))) {
{
int t_fnd2 = 0;
int32_t t_li = 1;
t_Val t_ov3 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])));
while (((t_li < ((ctron_list*)(t_cx_of(t_recv)))->n) && (!t_fnd2))) {
t_Val t_el = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_recv)))->items[t_li])));
if (((t_el).tag == 5)) {
{
if (t_str_contains((t_el).s, t_fmt(t_ov3))) {
{
t_fnd2 = 1;
}
}
}
}
else {
{
if (((t_el).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_el)))->items[0]), "L")) {
{
if (t_str_contains(t_fmt(t_el), t_fmt(t_ov3))) {
{
t_fnd2 = 1;
}
}
}
}
else {
{
if (t_veq(t_el, t_ov3)) {
{
t_fnd2 = 1;
}
}
}
}
}
}
else {
{
if (t_veq(t_el, t_ov3)) {
{
t_fnd2 = 1;
}
}
}
}
}
}
t_li += 1;
}
return t_e4("k", (t_ob).env, t_vB(t_fnd2), (t_ob).out);
}
}
}
}
ctron_list* t_ddx = (ctron_list*)(t_find_decl(t_file, t_m));
if ((((ctron_list*)(t_ddx))->n > 0)) {
{
ctron_list* t_fvv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_fvv), (char*)(t_v_box(t_recv)));
int32_t t_qq = 0;
while ((t_qq < ((ctron_list*)(t_vals))->n)) {
ctron_list_push((ctron_list*)(t_fvv), ((const char*)((ctron_list*)(t_vals))->items[t_qq]));
t_qq += 1;
}
return t_call_decl_vals(t_file, (t_ob).env, (t_ob).out, t_ddx, t_fvv);
}
}
ctron_panic(ctron_str_concat((const char*)("method:"), (const char*)(t_m)));
return t_e4("a", t_env, t_vV2(), t_out);
}
t_Flow t_eval_call(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_e) 
{
const char* t_ag = ((const char*)((ctron_list*)(t_e))->items[2]);
ctron_list* t_vals = (ctron_list*)(ctron_list_new());
ctron_list* t_env2 = (ctron_list*)(t_env);
const char* t_out2 = t_out;
int32_t t_i = 1;
while ((t_i < ctron_len((const void*)(t_ag)))) {
t_Flow t_ar = t_eval_expr(t_file, t_env2, t_out2, (ctron_list*)(((const char*)((ctron_list*)(t_ag))->items[t_i])));
if ((strcmp((const char*)((t_ar).kind), (const char*)("k")) != 0)) {
{
return t_ar;
}
}
t_env2 = (ctron_list*)((t_ar).env);
t_out2 = (t_ar).out;
ctron_list_push((ctron_list*)(t_vals), (char*)(t_v_box((t_ar).v)));
t_i += 1;
}
const char* t_cal = ((const char*)((ctron_list*)(t_e))->items[1]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Ident")) == 0)) {
{
return t_call_id(t_file, t_env2, t_out2, ((const char*)((ctron_list*)(t_cal))->items[1]), t_vals);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("Member")) == 0)) {
{
return t_call_mem(t_file, t_env2, t_out2, (ctron_list*)(t_cal), t_vals);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_cal))->items[0])), (const char*)("TypeArgs")) == 0)) {
{
const char* t_base = ((const char*)((ctron_list*)(t_cal))->items[1]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Member")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[3]), "as"))) {
{
t_Flow t_ar = t_eval_expr(t_file, t_env2, t_out2, (ctron_list*)(((const char*)((ctron_list*)(t_base))->items[1])));
if ((strcmp((const char*)((t_ar).kind), (const char*)("k")) != 0)) {
{
return t_ar;
}
}
const char* t_tgt = "";
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_cal))->items[2]))) > 1)) {
{
t_tgt = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_cal))->items[2])))->items[1])));
}
}
return t_e4("k", (t_ar).env, t_conv_as((t_ar).v, t_tgt), (t_ar).out);
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "Channel"))) {
{
const char* t_cap = ((const char*)((ctron_list*)(t_vals))->items[0]);
ctron_list* t_q = (ctron_list*)(ctron_list_new());
ctron_list* t_cur = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_cur), "M");
ctron_list_push((ctron_list*)(t_cur), "0");
ctron_list* t_tx = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_tx), "CH");
ctron_list_push((ctron_list*)(t_tx), (char*)(t_q));
ctron_list_push((ctron_list*)(t_tx), (char*)(t_cur));
ctron_list_push((ctron_list*)(t_tx), t_cap);
ctron_list* t_rx = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_rx), "CH");
ctron_list_push((ctron_list*)(t_rx), (char*)(t_q));
ctron_list_push((ctron_list*)(t_rx), (char*)(t_cur));
ctron_list_push((ctron_list*)(t_rx), t_cap);
ctron_list* t_pr = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_pr), "P");
ctron_list_push((ctron_list*)(t_pr), (char*)(t_v_box((t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_tx)})));
ctron_list_push((ctron_list*)(t_pr), (char*)(t_v_box((t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_rx)})));
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_pr)}, t_out2);
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "Global"))) {
{
ctron_list* t_gv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_gv), "M");
if ((((ctron_list*)(t_vals))->n > 1)) {
{
ctron_list_push((ctron_list*)(t_gv), ((const char*)((ctron_list*)(t_vals))->items[1]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_gv), (char*)(t_v_box(t_vV2())));
}
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_gv)}, t_out2);
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "Box"))) {
{
ctron_list* t_bv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_bv), "BOX");
if ((((ctron_list*)(t_vals))->n > 0)) {
{
ctron_list_push((ctron_list*)(t_bv), ((const char*)((ctron_list*)(t_vals))->items[0]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_bv), (char*)(t_v_box(t_vV2())));
}
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_bv)}, t_out2);
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "Mutex"))) {
{
ctron_list* t_mx = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_mx), "MUX");
if ((((ctron_list*)(t_vals))->n > 0)) {
{
ctron_list_push((ctron_list*)(t_mx), ((const char*)((ctron_list*)(t_vals))->items[0]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_mx), (char*)(t_v_box(t_vV2())));
}
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_mx)}, t_out2);
}
}
int t_islist = 0;
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "List"))) {
{
t_islist = 1;
}
}
if ((((((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Member")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[2]), "Nm")) && t_seq2(((const char*)((ctron_list*)(t_base))->items[3]), "list")) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_base))->items[1])))->items[0])), (const char*)("Ident")) == 0)) && t_seq2(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_base))->items[1])))->items[1]), "arena"))) {
{
t_islist = 1;
}
}
if (t_islist) {
{
return t_e4("k", t_env2, t_vL(), t_out2);
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0) && t_seq2(((const char*)((ctron_list*)(t_base))->items[1]), "Atomic"))) {
{
ctron_list* t_mv = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_mv), "M");
if ((((ctron_list*)(t_vals))->n > 0)) {
{
ctron_list_push((ctron_list*)(t_mv), ((const char*)((ctron_list*)(t_vals))->items[0]));
}
}
else {
{
ctron_list_push((ctron_list*)(t_mv), (char*)(t_v_box(t_vV2())));
}
}
return t_e4("k", t_env2, (t_Val){.tag = 8, .iv = 0, .b = 0, .s = "", .cx = t_cx_wrap(t_mv)}, t_out2);
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_base))->items[0])), (const char*)("Ident")) == 0)) {
{
return t_call_id(t_file, t_env, t_out, ((const char*)((ctron_list*)(t_base))->items[1]), t_vals);
}
}
}
}
ctron_panic("call target");
return t_e4("a", t_env, t_vV2(), t_out);
}
const char* t_bit_p2(int32_t t_k) 
{
const char* t_r = "1";
int32_t t_i = 0;
while ((t_i < t_k)) {
t_r = t_c6add(t_r, t_r);
t_i += 1;
}
return t_r;
}
const char* t_bit_quot(const char* t_qr) 
{
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_i) == 124)) {
{
return t_c6can(ctron_byte_slice(t_qr, 0, t_i));
}
}
t_i += 1;
}
return "0";
}
const char* t_bit_rest(const char* t_qr) 
{
int32_t t_i = 0;
while ((t_i < strlen((const char*)(t_qr)))) {
if ((ctron_byte_at(t_qr, t_i) == 124)) {
{
return t_c6can(ctron_byte_slice(t_qr, (t_i + 1), strlen((const char*)(t_qr))));
}
}
t_i += 1;
}
return "0";
}
const char* t_bit_umag(const char* t_t, int32_t t_bits) 
{
int t_neg = 0;
const char* t_m = t_t;
if (((strlen((const char*)(t_t)) > 0) && (ctron_byte_at(t_t, 0) == 45))) {
{
t_neg = 1;
t_m = ctron_byte_slice(t_t, 1, strlen((const char*)(t_t)));
}
}
const char* t_qr = t_c6divmod(t_c6can(t_m), t_bit_p2(t_bits));
t_m = t_bit_rest(t_qr);
if ((t_neg && (strcmp((const char*)(t_m), (const char*)("0")) != 0))) {
{
t_m = t_c6sub(t_bit_p2(t_bits), t_m);
}
}
return t_m;
}
const char* t_bit_sfold(const char* t_u, int32_t t_bits, int t_signed) 
{
if ((t_signed && (t_c6abscmp(t_u, t_bit_p2((t_bits - 1))) >= 0))) {
{
return t_c6sub(t_u, t_bit_p2(t_bits));
}
}
return t_u;
}
const char* t_bit_comb(int32_t t_bits, const char* t_a, const char* t_b, int32_t t_mode) 
{
const char* t_aa = t_a;
const char* t_bb = t_b;
const char* t_acc = "0";
const char* t_pw = "1";
int32_t t_i = 0;
{
const char* t_qa = 0;
const char* t_qb = 0;
int t_on = 0;
int t_b1 = 0;
int t_b2 = 0;
while ((t_i < t_bits)) {
if ((strcmp((const char*)(t_aa), (const char*)("0")) == 0)) {
{
if ((t_mode == 0)) {
{
return t_acc;
}
}
if (((strcmp((const char*)(t_aa), (const char*)("0")) == 0) && (strcmp((const char*)(t_bb), (const char*)("0")) == 0))) {
{
return t_acc;
}
}
}
}
t_qa = t_c6divmod(t_aa, "2");
t_qb = t_c6divmod(t_bb, "2");
t_on = 0;
t_b1 = (strcmp((const char*)(t_bit_rest(t_qa)), (const char*)("1")) == 0);
t_b2 = (strcmp((const char*)(t_bit_rest(t_qb)), (const char*)("1")) == 0);
if ((t_mode == 0)) {
{
t_on = (t_b1 && t_b2);
}
}
if ((t_mode == 1)) {
{
t_on = (t_b1 || t_b2);
}
}
if ((t_mode == 2)) {
{
t_on = (t_b1 != t_b2);
}
}
if (t_on) {
{
t_acc = t_c6add(t_acc, t_pw);
}
}
t_aa = t_bit_quot(t_qa);
t_bb = t_bit_quot(t_qb);
t_pw = t_c6add(t_pw, t_pw);
t_i += 1;
}
}
return t_acc;
}
int32_t t_bit_width(const char* t_w) 
{
if (((strcmp((const char*)(t_w), (const char*)("i32")) == 0) || (strcmp((const char*)(t_w), (const char*)("u32")) == 0))) {
{
return 32;
}
}
return 64;
}
int t_bit_signed(const char* t_w) 
{
return ((strcmp((const char*)(t_w), (const char*)("i32")) == 0) || (strcmp((const char*)(t_w), (const char*)("i64")) == 0));
}
t_Flow t_bit_ret(const char* t_w, const char* t_u, ctron_list* t_ob) 
{
if (t_seq2(t_w, "i32")) {
{
return t_e4("k", (ctron_list*)(((const char*)((ctron_list*)(t_ob))->items[1])), t_vI(t_dvi(t_u)), ((const char*)((ctron_list*)(t_ob))->items[3]));
}
}
if (t_seq2(t_w, "u32")) {
{
return t_e4("k", (ctron_list*)(((const char*)((ctron_list*)(t_ob))->items[1])), t_v6(t_bit_sfold(t_u, 32, 1)), ((const char*)((ctron_list*)(t_ob))->items[3]));
}
}
if (t_seq2(t_w, "i64")) {
{
return t_e4("k", (ctron_list*)(((const char*)((ctron_list*)(t_ob))->items[1])), t_v6(t_u), ((const char*)((ctron_list*)(t_ob))->items[3]));
}
}
return t_e4("k", (ctron_list*)(((const char*)((ctron_list*)(t_ob))->items[1])), t_v7(t_u), ((const char*)((ctron_list*)(t_ob))->items[3]));
}
t_Flow t_ns_bit(const char* t_m, ctron_list* t_vals, ctron_list* t_ob) 
{
if ((((ctron_list*)(t_vals))->n < 1)) {
{
ctron_panic("bit 实参");
}
}
if (t_or2((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[0])), (const char*)("I")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[0])), (const char*)("6")) == 0), t_or2((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[0])), (const char*)("7")) == 0), (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[0])), (const char*)("W")) == 0))))) {
{
}
}
else {
{
ctron_panic(ctron_str_concat((const char*)("bit 数值:"), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[0]))));
}
}
int32_t t_us = strlen((const char*)(t_m));
int32_t t_ui = 0;
while ((t_ui < strlen((const char*)(t_m)))) {
if ((ctron_byte_at(t_m, t_ui) == 95)) {
{
t_us = t_ui;
break;
}
}
t_ui += 1;
}
const char* t_op = ctron_byte_slice(t_m, 0, t_us);
const char* t_w = ctron_byte_slice(t_m, (t_us + 1), strlen((const char*)(t_m)));
if (t_or2((strcmp((const char*)(t_w), (const char*)("i32")) == 0), t_or2((strcmp((const char*)(t_w), (const char*)("u32")) == 0), t_or2((strcmp((const char*)(t_w), (const char*)("i64")) == 0), (strcmp((const char*)(t_w), (const char*)("u64")) == 0))))) {
{
}
}
else {
{
ctron_panic(ctron_str_concat((const char*)("bit 宽度:"), (const char*)(t_w)));
}
}
int32_t t_bits = t_bit_width(t_w);
int t_sgn = t_bit_signed(t_w);
const char* t_t1 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[0])))->items[1]);
if (t_or2(t_seq2(t_op, "and"), t_or2(t_seq2(t_op, "or"), t_seq2(t_op, "xor")))) {
{
if ((((ctron_list*)(t_vals))->n < 2)) {
{
ctron_panic("bit 实参");
}
}
int32_t t_md = 2;
if (t_seq2(t_op, "and")) {
{
t_md = 0;
}
}
if (t_seq2(t_op, "or")) {
{
t_md = 1;
}
}
const char* t_uc = t_bit_comb(t_bits, t_bit_umag(t_t1, t_bits), t_bit_umag(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])))->items[1]), t_bits), t_md);
return t_bit_ret(t_w, t_bit_sfold(t_uc, t_bits, t_sgn), t_ob);
}
}
if (t_seq2(t_op, "not")) {
{
const char* t_un = t_bit_comb(t_bits, t_bit_umag(t_t1, t_bits), t_c6sub(t_bit_p2(t_bits), "1"), 2);
return t_bit_ret(t_w, t_bit_sfold(t_un, t_bits, t_sgn), t_ob);
}
}
if (t_or2(t_seq2(t_op, "shl"), t_seq2(t_op, "shr"))) {
{
if ((((ctron_list*)(t_vals))->n < 2)) {
{
ctron_panic("bit 实参");
}
}
int32_t t_n = t_dvi(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_vals))->items[1])))->items[1]));
if (((t_n < 0) || (t_n >= t_bits))) {
{
ctron_panic("bit shift range");
}
}
const char* t_txt2 = "";
if (t_seq2(t_op, "shl")) {
{
const char* t_u2 = t_c6mul(t_bit_umag(t_t1, t_bits), t_bit_p2(t_n));
const char* t_qsh = t_c6divmod(t_u2, t_bit_p2(t_bits));
t_txt2 = t_bit_sfold(t_bit_rest(t_qsh), t_bits, t_sgn);
}
}
if (t_seq2(t_op, "shr")) {
{
if ((!t_sgn)) {
{
const char* t_ql = t_c6divmod(t_bit_umag(t_t1, t_bits), t_bit_p2(t_n));
t_txt2 = t_bit_quot(t_ql);
}
}
if (t_sgn) {
{
int t_neg2 = ((ctron_len((const void*)(t_t1)) > 0) && (ctron_byte_at(t_t1, 0) == 45));
const char* t_mg = t_t1;
if (t_neg2) {
{
t_mg = ctron_byte_slice(t_t1, 1, ctron_len((const void*)(t_t1)));
}
}
const char* t_qs = t_c6divmod(t_c6can(t_mg), t_bit_p2(t_n));
t_txt2 = t_bit_quot(t_qs);
if (t_neg2) {
{
const char* t_qc = t_c6divmod(t_c6add(t_c6can(t_mg), t_c6sub(t_bit_p2(t_n), "1")), t_bit_p2(t_n));
t_txt2 = ctron_str_concat((const char*)("-"), (const char*)(t_bit_quot(t_qc)));
}
}
}
}
}
}
return t_bit_ret(t_w, t_txt2, t_ob);
}
}
ctron_panic(ctron_str_concat((const char*)("bit 方法:"), (const char*)(t_m)));
return t_e4("a", (ctron_list*)(((const char*)((ctron_list*)(t_ob))->items[1])), t_vV2(), ((const char*)((ctron_list*)(t_ob))->items[3]));
}
t_Flow t_run_stmt(ctron_list* t_file, ctron_list* t_env, const char* t_out, ctron_list* t_st) 
{
const char* t_t = ((const char*)((ctron_list*)(t_st))->items[0]);
if ((strcmp((const char*)(t_t), (const char*)("Return")) == 0)) {
{
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])))->items[0])), (const char*)("None")) == 0)) {
{
return t_s4("r", t_env, t_out, t_vV2());
}
}
t_Flow t_er = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])));
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
return t_s4("r", (t_er).env, (t_er).out, (t_er).v);
}
}
if ((strcmp((const char*)(t_t), (const char*)("Break")) == 0)) {
{
return t_s4("b", t_env, t_out, t_vV2());
}
}
if ((strcmp((const char*)(t_t), (const char*)("Continue")) == 0)) {
{
return t_s4("c", t_env, t_out, t_vV2());
}
}
if ((strcmp((const char*)(t_t), (const char*)("Let")) == 0)) {
{
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[2]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
t_Flow t_er = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])));
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
t_Val t_vv = t_bind_of(t_file, (t_er).v);
if ((t_or2((((t_er).v).tag == 1), (((t_er).v).tag == 7)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])))->items[0])), (const char*)("Named")) == 0))) {
{
const char* t_h = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if (t_or2((strcmp((const char*)(t_h), (const char*)("U8")) == 0), t_or2((strcmp((const char*)(t_h), (const char*)("I8")) == 0), t_or2((strcmp((const char*)(t_h), (const char*)("U16")) == 0), (strcmp((const char*)(t_h), (const char*)("I16")) == 0))))) {
{
const char* t_wl = "u8";
if ((strcmp((const char*)(t_h), (const char*)("I8")) == 0)) {
{
t_wl = "i8";
}
}
if ((strcmp((const char*)(t_h), (const char*)("U16")) == 0)) {
{
t_wl = "u16";
}
}
if ((strcmp((const char*)(t_h), (const char*)("I16")) == 0)) {
{
t_wl = "i16";
}
}
t_vv = t_vW(t_wl, (int32_t)(((t_er).v).iv));
}
}
if (t_or2((strcmp((const char*)(t_h), (const char*)("U64")) == 0), (strcmp((const char*)(t_h), (const char*)("USize")) == 0))) {
{
t_vv = t_v7(t_dvi_str((int32_t)(((t_er).v).iv)));
}
}
if ((strcmp((const char*)(t_h), (const char*)("ISize")) == 0)) {
{
t_vv = t_v6(t_dvi_str((int32_t)(((t_er).v).iv)));
}
}
}
}
if (((((t_er).v).tag == 2) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])))->items[0])), (const char*)("Named")) == 0))) {
{
const char* t_h2 = t_ty_head((ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if (t_or2((strcmp((const char*)(t_h2), (const char*)("U64")) == 0), (strcmp((const char*)(t_h2), (const char*)("USize")) == 0))) {
{
t_vv = t_v7(t_v6t((t_er).v));
}
}
}
}
return t_s4("k", t_env_add((t_er).env, ((const char*)((ctron_list*)(t_pat))->items[1]), t_vv), (t_er).out, t_vV2());
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatWild")) == 0)) {
{
t_Flow t_er = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])));
if ((strcmp((const char*)((t_er).kind), (const char*)("k")) != 0)) {
{
return t_er;
}
}
return t_s4("k", (t_er).env, (t_er).out, t_vV2());
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatTup")) == 0)) {
{
t_Flow t_er2 = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[4])));
if ((strcmp((const char*)((t_er2).kind), (const char*)("k")) != 0)) {
{
return t_er2;
}
}
ctron_list* t_pmr = (ctron_list*)(t_pat_match((ctron_list*)(t_pat), (t_er2).v, (t_er2).env));
if (t_seq2(((const char*)((ctron_list*)(t_pmr))->items[0]), "1")) {
{
return t_s4("k", (ctron_list*)(((const char*)((ctron_list*)(t_pmr))->items[1])), (t_er2).out, t_vV2());
}
}
ctron_panic("let pat mismatch");
return t_s4("a", t_env, t_out, t_vV2());
}
}
ctron_panic(ctron_str_concat((const char*)("let pat:"), (const char*)(((const char*)((ctron_list*)(t_pat))->items[0]))));
return t_s4("a", t_env, t_out, t_vV2());
}
}
if ((strcmp((const char*)(t_t), (const char*)("Expr")) == 0)) {
{
t_Flow t_er = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])));
return t_er;
}
}
if ((strcmp((const char*)(t_t), (const char*)("Assign")) == 0)) {
{
const char* t_tg = ((const char*)((ctron_list*)(t_st))->items[1]);
if ((((strcmp((const char*)(((const char*)((ctron_list*)(t_tg))->items[0])), (const char*)("Member")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_tg))->items[2])), (const char*)("Nm")) == 0)) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_oname = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[1]);
const char* t_fname = ((const char*)((ctron_list*)(t_tg))->items[3]);
ctron_list* t_en = (ctron_list*)(t_env_at(t_env, t_oname));
if ((((ctron_list*)(t_en))->n == 0)) {
{
ctron_panic(ctron_str_concat((const char*)("unbound:"), (const char*)(t_oname)));
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Val t_ov = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_en))->items[1])));
if (((t_ov).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_ov)))->items[0]), "BOX")) {
{
t_ov = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_ov)))->items[1])));
}
}
}
}
if ((!(((t_ov).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_ov)))->items[0]), "U")))) {
{
ctron_panic("member assign target");
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_vr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if ((strcmp((const char*)((t_vr).kind), (const char*)("k")) != 0)) {
{
return t_vr;
}
}
const char* t_op = ((const char*)((ctron_list*)(t_st))->items[2]);
t_Val t_nv = (t_vr).v;
if ((!t_seq2(t_op, "Eq"))) {
{
ctron_list* t_oldf = (ctron_list*)(t_u_field(t_cx_of(t_ov), t_fname));
t_nv = t_vI(t_ari(t_op, (int32_t)((t_v_unbox(t_oldf)).iv), (int32_t)((t_nv).iv)));
}
}
if ((!t_u_set_ip(t_cx_of(t_ov), t_fname, t_v_box(t_nv)))) {
{
ctron_panic(ctron_str_concat((const char*)("field:"), (const char*)(t_fname)));
return t_s4("a", t_env, t_out, t_vV2());
}
}
return t_s4("k", (t_vr).env, (t_vr).out, t_vV2());
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tg))->items[0])), (const char*)("Ident")) == 0)) {
{
const char* t_nm = ((const char*)((ctron_list*)(t_tg))->items[1]);
ctron_list* t_en = (ctron_list*)(t_env_at(t_env, t_nm));
if ((((ctron_list*)(t_en))->n == 0)) {
{
ctron_panic(ctron_str_concat((const char*)("unbound:"), (const char*)(t_nm)));
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_vr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if ((strcmp((const char*)((t_vr).kind), (const char*)("k")) != 0)) {
{
return t_vr;
}
}
const char* t_op = ((const char*)((ctron_list*)(t_st))->items[2]);
t_Val t_nv = (t_vr).v;
if ((!t_seq2(t_op, "Eq"))) {
{
const char* t_oa = ((const char*)((ctron_list*)(t_en))->items[1]);
if (t_seq2(((const char*)((ctron_list*)(t_oa))->items[0]), "M")) {
{
((ctron_list*)(t_oa))->items[1] = (char*)(t_v_box(t_val_arith(t_op, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_oa))->items[1]))), t_nv)));
return t_s4("k", (t_vr).env, (t_vr).out, t_vV2());
}
}
t_nv = t_val_arith(t_op, t_v_unbox((ctron_list*)(t_oa)), t_nv);
}
}
return t_s4("k", t_env_set((t_vr).env, t_nm, t_bind_of(t_file, t_nv)), (t_vr).out, t_vV2());
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_tg))->items[0])), (const char*)("Index")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[0])), (const char*)("Ident")) == 0))) {
{
const char* t_nm3 = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[1]);
ctron_list* t_en3 = (ctron_list*)(t_env_at(t_env, t_nm3));
if ((((ctron_list*)(t_en3))->n == 0)) {
{
ctron_panic(ctron_str_concat((const char*)("unbound:"), (const char*)(t_nm3)));
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Val t_cont = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_en3))->items[1])));
if ((!(((t_cont).tag == 8) && t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_cont)))->items[0]), "A"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_cont)))->items[0]), "L"))))) {
{
ctron_panic("index assign target");
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_ixr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[2])));
if ((strcmp((const char*)((t_ixr).kind), (const char*)("k")) != 0)) {
{
return t_ixr;
}
}
int32_t t_idx = (int32_t)(((t_ixr).v).iv);
if (t_or2((t_idx < 0), (t_idx >= (((ctron_list*)(t_cx_of(t_cont)))->n - 1)))) {
{
const char* t_bnm = "?";
if ((ctron_len((const void*)(((const char*)((ctron_list*)(t_tg))->items[1]))) > 1)) {
{
t_bnm = ((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[1]);
}
}
ctron_panic(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("index out of bounds idx="), (const char*)(ctron_i32_to_string((int32_t)(t_idx))))), (const char*)(" base="))), (const char*)(t_bnm))), (const char*)(" contlen="))), (const char*)(ctron_i32_to_string((int32_t)((((ctron_list*)(t_cx_of(t_cont)))->n - 1))))));
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_vr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if ((strcmp((const char*)((t_vr).kind), (const char*)("k")) != 0)) {
{
return t_vr;
}
}
const char* t_op3 = ((const char*)((ctron_list*)(t_st))->items[2]);
t_Val t_nv3 = (t_vr).v;
if ((!t_seq2(t_op3, "Eq"))) {
{
t_Val t_old3 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_cont)))->items[(t_idx + 1)])));
t_nv3 = t_vI(t_ari(t_op3, (int32_t)((t_old3).iv), (int32_t)((t_nv3).iv)));
}
}
ctron_list* t_ccx = (ctron_list*)(t_cx_of(t_cont));
((ctron_list*)(t_ccx))->items[(t_idx + 1)] = (char*)(t_v_box(t_nv3));
return t_s4("k", (t_ixr).env, (t_ixr).out, t_vV2());
}
}
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_tg))->items[0])), (const char*)("Index")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[0])), (const char*)("Member")) == 0))) {
{
t_Flow t_mr = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])));
if ((strcmp((const char*)((t_mr).kind), (const char*)("k")) != 0)) {
{
return t_mr;
}
}
t_Val t_cont4 = (t_mr).v;
if ((!(((t_cont4).tag == 8) && t_or2(t_seq2(((const char*)((ctron_list*)(t_cx_of(t_cont4)))->items[0]), "A"), t_seq2(((const char*)((ctron_list*)(t_cx_of(t_cont4)))->items[0]), "L"))))) {
{
ctron_panic("index assign target");
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_ix4 = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[2])));
if ((strcmp((const char*)((t_ix4).kind), (const char*)("k")) != 0)) {
{
return t_ix4;
}
}
int32_t t_id4 = (int32_t)(((t_ix4).v).iv);
if (t_or2((t_id4 < 0), (t_id4 >= (((ctron_list*)(t_cx_of(t_cont4)))->n - 1)))) {
{
const char* t_b4 = "?";
if ((strcmp((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[1])))->items[0])), (const char*)("Ident")) == 0)) {
{
t_b4 = ctron_str_concat((const char*)(ctron_str_concat((const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[1])))->items[1])), (const char*)("."))), (const char*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_tg))->items[1])))->items[3])));
}
}
ctron_panic(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)("index out of bounds idx="), (const char*)(ctron_i32_to_string((int32_t)(t_id4))))), (const char*)(" base="))), (const char*)(t_b4))), (const char*)(" contlen="))), (const char*)(ctron_i32_to_string((int32_t)((((ctron_list*)(t_cx_of(t_cont4)))->n - 1))))));
return t_s4("a", t_env, t_out, t_vV2());
}
}
t_Flow t_vr4 = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])));
if ((strcmp((const char*)((t_vr4).kind), (const char*)("k")) != 0)) {
{
return t_vr4;
}
}
const char* t_op4 = ((const char*)((ctron_list*)(t_st))->items[2]);
t_Val t_nv4 = (t_vr4).v;
if ((!t_seq2(t_op4, "Eq"))) {
{
t_Val t_old4 = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_cont4)))->items[(t_id4 + 1)])));
t_nv4 = t_vI(t_ari(t_op4, (int32_t)((t_old4).iv), (int32_t)((t_nv4).iv)));
}
}
ctron_list* t_c4x = (ctron_list*)(t_cx_of(t_cont4));
((ctron_list*)(t_c4x))->items[(t_id4 + 1)] = (char*)(t_v_box(t_nv4));
return t_s4("k", (t_ix4).env, (t_ix4).out, t_vV2());
}
}
ctron_panic(ctron_str_concat((const char*)("assign target:"), (const char*)(((const char*)((ctron_list*)(t_tg))->items[0]))));
return t_s4("a", t_env, t_out, t_vV2());
}
}
if ((strcmp((const char*)(t_t), (const char*)("While")) == 0)) {
{
int t_go = 1;
ctron_list* t_envw = (ctron_list*)(t_env);
const char* t_outw = t_out;
int32_t t_base = ((ctron_list*)(t_envw))->n;
while (t_go) {
t_Flow t_cr = t_eval_expr(t_file, t_envw, t_outw, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[1])));
if ((strcmp((const char*)((t_cr).kind), (const char*)("k")) != 0)) {
{
return t_cr;
}
}
t_envw = (ctron_list*)((t_cr).env);
t_outw = (t_cr).out;
if (t_truth((t_cr).v)) {
{
t_Flow t_br = t_run_block(t_file, t_envw, t_outw, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])), 1);
if ((strcmp((const char*)((t_br).kind), (const char*)("b")) == 0)) {
{
t_envw = (ctron_list*)((t_br).env);
t_outw = (t_br).out;
t_go = 0;
}
}
else {
if ((strcmp((const char*)((t_br).kind), (const char*)("c")) == 0)) {
{
t_envw = (ctron_list*)((t_br).env);
t_outw = (t_br).out;
}
}
else {
if ((strcmp((const char*)((t_br).kind), (const char*)("k")) != 0)) {
{
return t_br;
}
}
else {
{
t_envw = (ctron_list*)((t_br).env);
t_outw = (t_br).out;
if ((((ctron_list*)(t_envw))->n > t_base)) {
{
t_envw = (ctron_list*)(t_env_dedupe(t_envw, (((ctron_list*)(t_envw))->n - t_base)));
}
}
}
}
}
}
}
}
else {
{
t_go = 0;
}
}
}
return t_s4("k", t_envw, t_outw, t_vV2());
}
}
if ((strcmp((const char*)(t_t), (const char*)("For")) == 0)) {
{
const char* t_pat = ((const char*)((ctron_list*)(t_st))->items[1]);
const char* t_nm = "_";
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_pat))->items[0])), (const char*)("PatId")) == 0)) {
{
t_nm = ((const char*)((ctron_list*)(t_pat))->items[1]);
}
}
t_Flow t_ir = t_eval_expr(t_file, t_env, t_out, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[2])));
if ((strcmp((const char*)((t_ir).kind), (const char*)("k")) != 0)) {
{
return t_ir;
}
}
t_Val t_it = (t_ir).v;
if (((t_it).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[0]), "U")) {
{
ctron_list* t_mth15 = (ctron_list*)(t_find_impl_method(t_file, ((const char*)((ctron_list*)(t_cx_of(t_it)))->items[1]), "next"));
if ((((ctron_list*)(t_mth15))->n > 0)) {
{
ctron_list* t_envI = (ctron_list*)(t_env_add((t_ir).env, "self", t_it));
const char* t_outI = (t_ir).out;
int t_goI = 1;
while (t_goI) {
t_Flow t_br15 = t_run_block(t_file, t_envI, t_outI, (ctron_list*)(((const char*)((ctron_list*)(t_mth15))->items[5])), 0);
if ((strcmp((const char*)((t_br15).kind), (const char*)("a")) == 0)) {
{
return t_br15;
}
}
t_envI = (ctron_list*)((t_br15).env);
t_outI = (t_br15).out;
if ((((t_br15).v).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of((t_br15).v)))->items[0]), "T")) {
{
if ((((ctron_list*)(t_cx_of((t_br15).v)))->n > 2)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of((t_br15).v)))->items[1]), "Some")) {
{
ctron_list* t_eit = (ctron_list*)(t_env_add(t_envI, t_nm, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of((t_br15).v)))->items[2])))));
t_Flow t_brb = t_run_block(t_file, t_eit, t_outI, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), 0);
if ((strcmp((const char*)((t_brb).kind), (const char*)("b")) == 0)) {
{
return t_s4("k", t_env_drop((t_brb).env, 1), (t_brb).out, t_vV2());
}
}
if (((strcmp((const char*)((t_brb).kind), (const char*)("k")) != 0) && (strcmp((const char*)((t_brb).kind), (const char*)("c")) != 0))) {
{
return t_brb;
}
}
t_envI = (ctron_list*)(t_env_drop((t_brb).env, 1));
t_outI = (t_brb).out;
}
}
else {
{
t_goI = 0;
}
}
}
}
else {
{
t_goI = 0;
}
}
}
}
else {
{
t_goI = 0;
}
}
}
}
else {
{
t_goI = 0;
}
}
}
return t_s4("k", t_envI, t_outI, t_vV2());
}
}
}
}
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[0]), "A")) {
{
ctron_list* t_envw3 = (ctron_list*)((t_ir).env);
const char* t_outw3 = (t_ir).out;
int32_t t_c2 = 1;
while ((t_c2 < ((ctron_list*)(t_cx_of(t_it)))->n)) {
ctron_list* t_eit = (ctron_list*)(t_env_add(t_envw3, t_nm, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[t_c2])))));
t_Flow t_br = t_run_block(t_file, t_eit, t_outw3, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), 0);
if ((strcmp((const char*)((t_br).kind), (const char*)("b")) == 0)) {
{
return t_s4("k", t_env_drop((t_br).env, 1), (t_br).out, t_vV2());
}
}
if (((strcmp((const char*)((t_br).kind), (const char*)("k")) != 0) && (strcmp((const char*)((t_br).kind), (const char*)("c")) != 0))) {
{
return t_br;
}
}
t_envw3 = (ctron_list*)(t_env_drop((t_br).env, 1));
t_outw3 = (t_br).out;
t_c2 += 1;
}
return t_s4("k", t_envw3, t_outw3, t_vV2());
}
}
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[0]), "L")) {
{
ctron_list* t_envl = (ctron_list*)((t_ir).env);
const char* t_outl = (t_ir).out;
int32_t t_ci = 1;
while ((t_ci < ((ctron_list*)(t_cx_of(t_it)))->n)) {
ctron_list* t_eit = (ctron_list*)(t_env_add(t_envl, t_nm, t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[t_ci])))));
t_Flow t_br = t_run_block(t_file, t_eit, t_outl, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), 0);
if ((strcmp((const char*)((t_br).kind), (const char*)("b")) == 0)) {
{
return t_s4("k", t_env_drop((t_br).env, 1), (t_br).out, t_vV2());
}
}
if (((strcmp((const char*)((t_br).kind), (const char*)("k")) != 0) && (strcmp((const char*)((t_br).kind), (const char*)("c")) != 0))) {
{
return t_br;
}
}
t_envl = (ctron_list*)(t_env_drop((t_br).env, 1));
t_outl = (t_br).out;
t_ci += 1;
}
return t_s4("k", t_envl, t_outl, t_vV2());
}
}
}
}
if ((!(((t_it).tag == 8) && t_seq2(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[0]), "R")))) {
{
ctron_panic("for iter not range");
return t_s4("a", t_env, t_out, t_vV2());
}
}
int32_t t_lo = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[2])))).iv);
int32_t t_hi = (int32_t)((t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[3])))).iv);
int t_incl = t_seq2(((const char*)((ctron_list*)(t_cx_of(t_it)))->items[1]), "true");
ctron_list* t_envw = (ctron_list*)((t_ir).env);
const char* t_outw = (t_ir).out;
int32_t t_cur = t_lo;
int t_more = 1;
if (t_incl) {
{
if ((t_cur > t_hi)) {
{
t_more = 0;
}
}
}
}
else {
{
if ((t_cur >= t_hi)) {
{
t_more = 0;
}
}
}
}
while (t_more) {
ctron_list* t_eit = (ctron_list*)(t_env_add(t_envw, t_nm, t_vI(t_cur)));
t_Flow t_br = t_run_block(t_file, t_eit, t_outw, (ctron_list*)(((const char*)((ctron_list*)(t_st))->items[3])), 0);
if ((strcmp((const char*)((t_br).kind), (const char*)("k")) != 0)) {
{
return t_br;
}
}
t_envw = (ctron_list*)(t_env_drop((t_br).env, 1));
t_outw = (t_br).out;
t_cur += 1;
if (t_incl) {
{
if ((t_cur > t_hi)) {
{
t_more = 0;
}
}
}
}
else {
{
if ((t_cur >= t_hi)) {
{
t_more = 0;
}
}
}
}
}
return t_s4("k", t_envw, t_outw, t_vV2());
}
}
ctron_panic(ctron_str_concat((const char*)("stmt:"), (const char*)(t_t)));
return t_s4("a", t_env, t_out, t_vV2());
}
t_Flow t_run_block(ctron_list* t_file, ctron_list* t_env0, const char* t_out, ctron_list* t_blk, int t_keep) 
{
const char* t_flow = "k";
ctron_list* t_env = (ctron_list*)(t_env0);
const char* t_o2 = t_out;
t_Val t_val = t_vV2();
int32_t t_i = 1;
while ((t_i < (((ctron_list*)(t_blk))->n - 1))) {
if ((strcmp((const char*)(t_flow), (const char*)("k")) == 0)) {
{
t_Flow t_sr = t_run_stmt(t_file, t_env, t_o2, (ctron_list*)(((const char*)((ctron_list*)(t_blk))->items[t_i])));
t_flow = (t_sr).kind;
t_env = (ctron_list*)((t_sr).env);
t_o2 = (t_sr).out;
t_val = (t_sr).v;
}
}
t_i += 1;
}
if ((strcmp((const char*)(t_flow), (const char*)("k")) == 0)) {
{
const char* t_tl = ((const char*)((ctron_list*)(t_blk))->items[(((ctron_list*)(t_blk))->n - 1)]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_tl))->items[0])), (const char*)("None")) == 0)) {
{
t_val = t_vV2();
}
}
else {
{
t_Flow t_er = t_eval_expr(t_file, t_env, t_o2, (ctron_list*)(t_tl));
t_flow = (t_er).kind;
t_env = (ctron_list*)((t_er).env);
t_val = (t_er).v;
t_o2 = (t_er).out;
}
}
}
}
int32_t t_extra = (((ctron_list*)(t_env))->n - ((ctron_list*)(t_env0))->n);
if ((t_extra > 0)) {
{
if (t_keep) {
{
t_env = (ctron_list*)(t_env_dedupe(t_env, t_extra));
}
}
else {
{
int32_t t_di = 0;
while ((t_di < t_extra)) {
t_Val t_dv = t_v_unbox((ctron_list*)(((const char*)((ctron_list*)(((const char*)((ctron_list*)(t_env))->items[t_di])))->items[1])));
if (((t_dv).tag == 8)) {
{
if (t_seq2(((const char*)((ctron_list*)(t_cx_of(t_dv)))->items[0]), "U")) {
{
ctron_list* t_dm = (ctron_list*)(t_find_impl_method(t_file, ((const char*)((ctron_list*)(t_cx_of(t_dv)))->items[1]), "drop"));
if ((((ctron_list*)(t_dm))->n > 0)) {
{
ctron_list* t_dnenv = (ctron_list*)(t_env_add(ctron_list_new(), "self", t_dv));
t_Flow t_dbr = t_run_block(t_file, t_dnenv, t_o2, (ctron_list*)(((const char*)((ctron_list*)(t_dm))->items[5])), 0);
t_o2 = (t_dbr).out;
}
}
}
}
}
}
t_di += 1;
}
t_env = (ctron_list*)(t_env_drop(t_env, t_extra));
}
}
}
}
return t_s4(t_flow, t_env, t_o2, t_val);
}
ctron_list* t_statics_env(ctron_list* t_file) 
{
ctron_list* t_env = (ctron_list*)(ctron_list_new());
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if (((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Static")) == 0) && (strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[2])), (const char*)("false")) == 0))) {
{
t_Flow t_vr = t_eval_expr(t_file, t_env, "", (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[4])));
if ((strcmp((const char*)((t_vr).kind), (const char*)("k")) == 0)) {
{
t_env = (ctron_list*)(t_env_add(t_env, ((const char*)((ctron_list*)(t_d))->items[1]), t_bind_of(t_file, (t_vr).v)));
}
}
}
}
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Const")) == 0)) {
{
t_Flow t_vr2 = t_eval_expr(t_file, t_env, "", (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[3])));
if ((strcmp((const char*)((t_vr2).kind), (const char*)("k")) == 0)) {
{
t_env = (ctron_list*)(t_env_add(t_env, ((const char*)((ctron_list*)(t_d))->items[1]), t_bind_of(t_file, (t_vr2).v)));
}
}
}
}
t_i += 1;
}
}
return t_env;
}
t_Flow t_run_tests(ctron_list* t_file) 
{
ctron_list* t_env0 = (ctron_list*)(t_statics_env(t_file));
const char* t_acc = "";
int32_t t_i = 1;
{
const char* t_d = 0;
while ((t_i < ((ctron_list*)(t_file))->n)) {
t_d = ((const char*)((ctron_list*)(t_file))->items[t_i]);
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_d))->items[0])), (const char*)("Test")) == 0)) {
{
t_Flow t_br = t_run_block(t_file, t_env0, "", (ctron_list*)(((const char*)((ctron_list*)(t_d))->items[2])), 0);
if ((strcmp((const char*)((t_br).kind), (const char*)("a")) == 0)) {
{
return t_s4("a", (t_br).env, ctron_str_concat((const char*)(t_acc), (const char*)((t_br).out)), (t_br).v);
}
}
t_acc = ctron_str_concat((const char*)(t_acc), (const char*)((t_br).out));
}
}
t_i += 1;
}
}
return t_s4("k", ctron_list_new(), t_acc, t_vV2());
}
int t_fmt_kw(const char* t_t) 
{
ctron_list* t_ks = (ctron_list*)(ctron_list_new());
ctron_list_push((ctron_list*)(t_ks), "fn");
ctron_list_push((ctron_list*)(t_ks), "let");
ctron_list_push((ctron_list*)(t_ks), "var");
ctron_list_push((ctron_list*)(t_ks), "const");
ctron_list_push((ctron_list*)(t_ks), "static");
ctron_list_push((ctron_list*)(t_ks), "comptime");
ctron_list_push((ctron_list*)(t_ks), "if");
ctron_list_push((ctron_list*)(t_ks), "else");
ctron_list_push((ctron_list*)(t_ks), "match");
ctron_list_push((ctron_list*)(t_ks), "while");
ctron_list_push((ctron_list*)(t_ks), "for");
ctron_list_push((ctron_list*)(t_ks), "in");
ctron_list_push((ctron_list*)(t_ks), "break");
ctron_list_push((ctron_list*)(t_ks), "continue");
ctron_list_push((ctron_list*)(t_ks), "return");
ctron_list_push((ctron_list*)(t_ks), "struct");
ctron_list_push((ctron_list*)(t_ks), "class");
ctron_list_push((ctron_list*)(t_ks), "enum");
ctron_list_push((ctron_list*)(t_ks), "trait");
ctron_list_push((ctron_list*)(t_ks), "impl");
ctron_list_push((ctron_list*)(t_ks), "own");
ctron_list_push((ctron_list*)(t_ks), "scope");
ctron_list_push((ctron_list*)(t_ks), "test");
ctron_list_push((ctron_list*)(t_ks), "use");
ctron_list_push((ctron_list*)(t_ks), "pub");
ctron_list_push((ctron_list*)(t_ks), "extern");
ctron_list_push((ctron_list*)(t_ks), "prop");
ctron_list_push((ctron_list*)(t_ks), "true");
ctron_list_push((ctron_list*)(t_ks), "false");
ctron_list_push((ctron_list*)(t_ks), "void");
ctron_list_push((ctron_list*)(t_ks), "self");
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_ks))->n)) {
if ((strcmp((const char*)(((const char*)((ctron_list*)(t_ks))->items[t_k])), (const char*)(t_t)) == 0)) {
{
return 1;
}
}
t_k += 1;
}
return 0;
}
int t_fmt_is_digit0(const char* t_t) 
{
if ((strlen((const char*)(t_t)) == 0)) {
{
return 0;
}
}
return t_is_digit(ctron_byte_at(t_t, 0));
}
int t_fmt_identk(const char* t_t, const char* t_before) 
{
if ((strlen((const char*)(t_t)) == 0)) {
{
return 0;
}
}
if ((!t_is_al(ctron_byte_at(t_t, 0)))) {
{
return 0;
}
}
if (t_fmt_is_digit0(t_t)) {
{
return 0;
}
}
if ((strcmp((const char*)(t_t), (const char*)("or")) == 0)) {
{
return (strcmp((const char*)(t_before), (const char*)(".")) == 0);
}
}
if (t_fmt_kw(t_t)) {
{
return 0;
}
}
return 1;
}
int t_fmt_operand_end(const char* t_prev, int t_has_prev, const char* t_prev2) 
{
if ((!t_has_prev)) {
{
return 0;
}
}
if (t_or2((strcmp((const char*)(t_prev), (const char*)(")")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("]")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("}")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("?")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("_")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("true")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("false")) == 0), (strcmp((const char*)(t_prev), (const char*)("self")) == 0))))))))) {
{
return 1;
}
}
if (t_or2(t_fmt_is_digit0(t_prev), (ctron_byte_at(t_prev, 0) == 34))) {
{
return 1;
}
}
if (t_fmt_identk(t_prev, t_prev2)) {
{
return 1;
}
}
return 0;
}
int t_fmt_needs_space(const char* t_prev, int t_has_prev, const char* t_prev2, const char* t_cur, int t_pipe_open, int t_sign_unary) 
{
if ((!t_has_prev)) {
{
return 0;
}
}
if (t_or2((strcmp((const char*)(t_cur), (const char*)(",")) == 0), t_or2((strcmp((const char*)(t_cur), (const char*)(":")) == 0), t_or2((strcmp((const char*)(t_cur), (const char*)(")")) == 0), t_or2((strcmp((const char*)(t_cur), (const char*)("]")) == 0), t_or2((strcmp((const char*)(t_cur), (const char*)(".")) == 0), t_or2((strcmp((const char*)(t_cur), (const char*)("?")) == 0), (strcmp((const char*)(t_cur), (const char*)("...")) == 0)))))))) {
{
return 0;
}
}
if (((strcmp((const char*)(t_cur), (const char*)("}")) == 0) && (strcmp((const char*)(t_prev), (const char*)("{")) == 0))) {
{
return 0;
}
}
if ((strcmp((const char*)(t_cur), (const char*)("[")) == 0)) {
{
if (t_or2(t_fmt_identk(t_prev, t_prev2), t_or2((strcmp((const char*)(t_prev), (const char*)(")")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("]")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("?")) == 0), (strcmp((const char*)(t_prev), (const char*)("#")) == 0)))))) {
{
return 0;
}
}
}
}
if ((strcmp((const char*)(t_cur), (const char*)("(")) == 0)) {
{
if (t_or2(t_fmt_identk(t_prev, t_prev2), t_or2((strcmp((const char*)(t_prev), (const char*)(")")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("]")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("?")) == 0), (strcmp((const char*)(t_prev), (const char*)("self")) == 0)))))) {
{
return 0;
}
}
}
}
if (t_or2((strcmp((const char*)(t_prev), (const char*)("(")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("[")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)(".")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("@")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("#")) == 0), (strcmp((const char*)(t_prev), (const char*)("?")) == 0))))))) {
{
return 0;
}
}
if (t_or2((strcmp((const char*)(t_prev), (const char*)("!")) == 0), (strcmp((const char*)(t_prev), (const char*)("&")) == 0))) {
{
return 0;
}
}
if (t_or2((strcmp((const char*)(t_prev), (const char*)("-")) == 0), (strcmp((const char*)(t_prev), (const char*)("+")) == 0))) {
{
if (t_sign_unary) {
{
return 0;
}
}
}
}
if ((strcmp((const char*)(t_prev), (const char*)("|")) == 0)) {
{
if (t_pipe_open) {
{
return 0;
}
}
}
}
if (t_or2((strcmp((const char*)(t_prev), (const char*)("..")) == 0), (strcmp((const char*)(t_prev), (const char*)("..=")) == 0))) {
{
return 0;
}
}
if (t_or2((strcmp((const char*)(t_cur), (const char*)("..")) == 0), (strcmp((const char*)(t_cur), (const char*)("..=")) == 0))) {
{
return 0;
}
}
return 1;
}
int32_t t_fmt_free_nl(const char* t_src, ctron_list* t_cstarts, ctron_list* t_cends, int32_t t_from, int32_t t_to) 
{
if ((t_to <= t_from)) {
{
return 0;
}
}
if ((t_to > strlen((const char*)(t_src)))) {
{
t_to = strlen((const char*)(t_src));
}
}
int32_t t_count = 0;
int32_t t_i = t_from;
while ((t_i < t_to)) {
if ((ctron_byte_at(t_src, t_i) == 10)) {
{
int t_covered = 0;
int32_t t_k = 0;
while ((t_k < ((ctron_list*)(t_cstarts))->n)) {
if ((((int32_t)(long)((ctron_list*)(t_cstarts))->items[t_k]) > t_i)) {
{
t_k = ((ctron_list*)(t_cstarts))->n;
}
}
else {
{
if ((t_i < ((int32_t)(long)((ctron_list*)(t_cends))->items[t_k]))) {
{
t_covered = 1;
}
}
t_k += 1;
}
}
}
if ((!t_covered)) {
{
t_count += 1;
}
}
}
}
t_i += 1;
}
return t_count;
}
int t_fmt_block_ml(const char* t_src, ctron_list* t_tstarts, ctron_list* t_tends, ctron_list* t_partner, int32_t t_i) 
{
int32_t t_p = ((int32_t)(long)((ctron_list*)(t_partner))->items[t_i]);
if ((t_p < 0)) {
{
return 0;
}
}
int32_t t_s = ((int32_t)(long)((ctron_list*)(t_tends))->items[t_i]);
int32_t t_e = ((int32_t)(long)((ctron_list*)(t_tstarts))->items[t_p]);
if ((t_e > strlen((const char*)(t_src)))) {
{
t_e = strlen((const char*)(t_src));
}
}
int32_t t_j = t_s;
while ((t_j < t_e)) {
if ((ctron_byte_at(t_src, t_j) == 10)) {
{
return 1;
}
}
t_j += 1;
}
return 0;
}
const char* t_fmt_ind(int32_t t_n) 
{
const char* t_out = "";
int32_t t_k = 0;
while ((t_k < t_n)) {
t_out = ctron_str_concat((const char*)(t_out), (const char*)("    "));
t_k += 1;
}
return t_out;
}
const char* t_fmt_src(const char* t_src, ctron_list* t_diags) 
{
ctron_list* t_toks = (ctron_list*)(ctron_list_new());
ctron_list* t_lns = (ctron_list*)(ctron_list_new());
ctron_list* t_cols = (ctron_list*)(ctron_list_new());
ctron_list* t_pd = (ctron_list*)(ctron_list_new());
ctron_list* t_tstarts = (ctron_list*)(ctron_list_new());
ctron_list* t_tends = (ctron_list*)(ctron_list_new());
ctron_list* t_cstarts = (ctron_list*)(ctron_list_new());
ctron_list* t_cends = (ctron_list*)(ctron_list_new());
t_scan5(t_src, t_toks, t_lns, t_cols, t_pd, t_tstarts, t_tends, t_cstarts, t_cends);
if ((((ctron_list*)(t_pd))->n > 0)) {
{
int32_t t_d = 0;
while ((t_d < ((ctron_list*)(t_pd))->n)) {
ctron_list_push((ctron_list*)(t_diags), ((const char*)((ctron_list*)(t_pd))->items[t_d]));
t_d += 1;
}
return "";
}
}
int32_t t_n = ((ctron_list*)(t_toks))->n;
ctron_list* t_partner = (ctron_list*)(ctron_list_new());
int32_t t_pi = 0;
while ((t_pi < t_n)) {
ctron_list_push((ctron_list*)(t_partner), (const char*)(long)((-1)));
t_pi += 1;
}
ctron_list* t_stack = (ctron_list*)(ctron_list_new());
int32_t t_slen = 0;
int32_t t_qi = 0;
{
const char* t_qt = 0;
while ((t_qi < t_n)) {
t_qt = ((const char*)((ctron_list*)(t_toks))->items[t_qi]);
if ((strcmp((const char*)(t_qt), (const char*)("{")) == 0)) {
{
if ((t_slen < ((ctron_list*)(t_stack))->n)) {
{
((ctron_list*)(t_stack))->items[t_slen] = (const char*)(long)(t_qi);
}
}
else {
{
ctron_list_push((ctron_list*)(t_stack), (const char*)(long)(t_qi));
}
}
t_slen += 1;
}
}
else {
if ((strcmp((const char*)(t_qt), (const char*)("}")) == 0)) {
{
if ((t_slen > 0)) {
{
t_slen -= 1;
int32_t t_open = ((int32_t)(long)((ctron_list*)(t_stack))->items[t_slen]);
((ctron_list*)(t_partner))->items[t_open] = (const char*)(long)(t_qi);
((ctron_list*)(t_partner))->items[t_qi] = (const char*)(long)(t_open);
}
}
}
}
}
t_qi += 1;
}
}
const char* t_out = "";
int32_t t_indent = 0;
int t_at_ls = 1;
int32_t t_extra = 0;
int32_t t_last_end = 0;
const char* t_prev = "";
const char* t_prev2 = "";
int t_has_prev = 0;
int t_pipe_open = 0;
int t_sign_unary = 0;
int32_t t_ci = 0;
int32_t t_nc = ((ctron_list*)(t_cstarts))->n;
int32_t t_i = 0;
{
const char* t_t = 0;
int32_t t_ts = 0;
int32_t t_te = 0;
const char* t_text = 0;
while ((t_i < t_n)) {
t_t = ((const char*)((ctron_list*)(t_toks))->items[t_i]);
if ((strcmp((const char*)(t_t), (const char*)("NL")) == 0)) {
{
t_i += 1;
continue;
}
}
t_ts = ((int32_t)(long)((ctron_list*)(t_tstarts))->items[t_i]);
t_te = ((int32_t)(long)((ctron_list*)(t_tends))->items[t_i]);
int t_more = 1;
while (t_more) {
t_more = 0;
if (((t_ci < t_nc) && (((int32_t)(long)((ctron_list*)(t_cstarts))->items[t_ci]) < t_ts))) {
{
int32_t t_cs = ((int32_t)(long)((ctron_list*)(t_cstarts))->items[t_ci]);
int32_t t_ce = ((int32_t)(long)((ctron_list*)(t_cends))->items[t_ci]);
t_ci += 1;
int32_t t_tec = t_ce;
while (((t_tec > t_cs) && t_or2((ctron_byte_at(t_src, (t_tec - 1)) == 32), t_or2((ctron_byte_at(t_src, (t_tec - 1)) == 9), (ctron_byte_at(t_src, (t_tec - 1)) == 13))))) {
t_tec -= 1;
}
int32_t t_pf = t_fmt_free_nl(t_src, t_cstarts, t_cends, t_last_end, t_cs);
if ((t_pf >= 1)) {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
if ((t_pf >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
t_at_ls = 1;
t_extra = 0;
}
}
else {
if ((t_pf >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_src, t_cs, t_tec)));
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
t_at_ls = 1;
}
}
else {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_src, t_cs, t_tec)));
}
}
t_last_end = t_ce;
t_more = 1;
}
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("else")) == 0), (strcmp((const char*)(t_t), (const char*)(".")) == 0))) {
{
}
}
else {
{
int32_t t_free = t_fmt_free_nl(t_src, t_cstarts, t_cends, t_last_end, t_ts);
if ((t_free >= 1)) {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
if ((t_free >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
t_at_ls = 1;
t_extra = 0;
}
}
else {
if ((t_free >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
}
}
}
}
}
if ((strcmp((const char*)(t_t), (const char*)("{")) == 0)) {
{
if (((!t_at_ls) && t_fmt_needs_space(t_prev, t_has_prev, t_prev2, t_t, t_pipe_open, t_sign_unary))) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("{"));
if (t_fmt_block_ml(t_src, t_tstarts, t_tends, t_partner, t_i)) {
{
t_indent += 1;
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
t_at_ls = 1;
t_extra = 0;
}
}
t_prev2 = t_prev;
t_prev = t_t;
t_has_prev = 1;
t_last_end = t_te;
t_i += 1;
continue;
}
}
if ((strcmp((const char*)(t_t), (const char*)("}")) == 0)) {
{
int t_ml = 0;
if ((((int32_t)(long)((ctron_list*)(t_partner))->items[t_i]) >= 0)) {
{
t_ml = t_fmt_block_ml(t_src, t_tstarts, t_tends, t_partner, ((int32_t)(long)((ctron_list*)(t_partner))->items[t_i]));
}
}
if ((t_ml && (t_indent > 0))) {
{
t_indent -= 1;
}
}
if ((((!t_at_ls) && (!t_ml)) && t_fmt_needs_space(t_prev, t_has_prev, t_prev2, t_t, t_pipe_open, t_sign_unary))) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("}"));
t_prev2 = t_prev;
t_prev = t_t;
t_has_prev = 1;
t_last_end = t_te;
t_i += 1;
continue;
}
}
if ((strcmp((const char*)(t_t), (const char*)("else")) == 0)) {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("else"));
t_prev2 = t_prev;
t_prev = t_t;
t_has_prev = 1;
t_last_end = t_te;
t_i += 1;
continue;
}
}
if ((strcmp((const char*)(t_t), (const char*)("|")) == 0)) {
{
int t_psp = 1;
if (t_pipe_open) {
{
t_psp = 0;
t_pipe_open = 0;
}
}
else {
{
if (t_or2((strcmp((const char*)(t_prev), (const char*)(",")) == 0), t_or2((strcmp((const char*)(t_prev), (const char*)("(")) == 0), t_or2(t_fmt_identk(t_prev, t_prev2), t_or2((strcmp((const char*)(t_prev), (const char*)(")")) == 0), (strcmp((const char*)(t_prev), (const char*)("[")) == 0)))))) {
{
t_psp = 0;
}
}
t_pipe_open = 1;
}
}
if (((!t_at_ls) && t_psp)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)("|"));
t_prev2 = t_prev;
t_prev = t_t;
t_has_prev = 1;
t_last_end = t_te;
t_i += 1;
continue;
}
}
if ((strcmp((const char*)(t_t), (const char*)(".")) == 0)) {
{
int32_t t_free = t_fmt_free_nl(t_src, t_cstarts, t_cends, t_last_end, t_ts);
if ((t_free >= 1)) {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
if ((t_free >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
t_at_ls = 1;
t_extra = 0;
}
}
else {
if ((t_free >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
}
t_extra = 1;
}
}
}
}
if (t_or2((strcmp((const char*)(t_t), (const char*)("-")) == 0), (strcmp((const char*)(t_t), (const char*)("+")) == 0))) {
{
t_sign_unary = (!t_fmt_operand_end(t_prev, t_has_prev, t_prev2));
}
}
t_text = t_t;
if (t_fmt_is_digit0(t_t)) {
{
t_text = ctron_str_concat((const char*)(t_num_text(t_t)), (const char*)(t_num_sfx(t_t)));
}
}
if (((!t_at_ls) && t_fmt_needs_space(t_prev, t_has_prev, t_prev2, t_t, t_pipe_open, t_sign_unary))) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_text));
t_prev2 = t_prev;
t_prev = t_t;
t_has_prev = 1;
t_last_end = t_te;
t_i += 1;
}
}
{
int32_t t_cs = 0;
int32_t t_ce = 0;
int32_t t_pf = 0;
while ((t_ci < t_nc)) {
t_cs = ((int32_t)(long)((ctron_list*)(t_cstarts))->items[t_ci]);
t_ce = ((int32_t)(long)((ctron_list*)(t_cends))->items[t_ci]);
t_ci += 1;
int32_t t_tec = t_ce;
while (((t_tec > t_cs) && t_or2((ctron_byte_at(t_src, (t_tec - 1)) == 32), t_or2((ctron_byte_at(t_src, (t_tec - 1)) == 9), (ctron_byte_at(t_src, (t_tec - 1)) == 13))))) {
t_tec -= 1;
}
t_pf = t_fmt_free_nl(t_src, t_cstarts, t_cends, t_last_end, t_cs);
if ((t_pf >= 1)) {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
if ((t_pf >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
t_at_ls = 1;
t_extra = 0;
}
}
else {
if ((t_pf >= 2)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
}
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_src, t_cs, t_tec)));
t_out = ctron_str_concat((const char*)(t_out), (const char*)("\n"));
t_at_ls = 1;
}
}
else {
{
if ((!t_at_ls)) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(" "));
}
}
if (t_at_ls) {
{
t_out = ctron_str_concat((const char*)(t_out), (const char*)(t_fmt_ind((t_indent + t_extra))));
t_at_ls = 0;
}
}
t_out = ctron_str_concat((const char*)(t_out), (const char*)(ctron_byte_slice(t_src, t_cs, t_tec)));
}
}
t_last_end = t_ce;
}
}
int32_t t_s = 0;
int32_t t_e = strlen((const char*)(t_out));
while (((t_s < t_e) && (ctron_byte_at(t_out, t_s) == 10))) {
t_s += 1;
}
while (((t_e > t_s) && t_or2((ctron_byte_at(t_out, (t_e - 1)) == 10), t_or2((ctron_byte_at(t_out, (t_e - 1)) == 32), (ctron_byte_at(t_out, (t_e - 1)) == 9))))) {
t_e -= 1;
}
if ((t_e == t_s)) {
{
return "";
}
}
return ctron_str_concat((const char*)(ctron_byte_slice(t_out, t_s, t_e)), (const char*)("\n"));
}
int32_t t_main() 
{
const char* t_path = "ANCHORINPUT";
const char* t_src = ctron_read_file_cli(ctron_anchor);
const char* t_mv = (const char*)(t_src);
if (t_mv != NULL) {
const char* t_s = t_mv;
{
const char* t_epath = ctron_entry();
if (t_seq2(t_epath, "")) {
{
t_epath = t_path;
}
}
ctron_list* t_diags = (ctron_list*)(ctron_list_new());
const char* t_out = t_fmt_src(t_s, t_diags);
if ((((ctron_list*)(t_diags))->n > 0)) {
{
int32_t t_i = 0;
while ((t_i < ((ctron_list*)(t_diags))->n)) {
printf("%s\n", (const char*)(ctron_str_concat((const char*)(ctron_str_concat((const char*)(t_epath), (const char*)(":"))), (const char*)(((const char*)((ctron_list*)(t_diags))->items[t_i])))));
t_i += 1;
}
return 1;
}
}
printf("%s", (const char*)(t_out));
return 0;
}
}
else {
{
printf("%s\n", (const char*)("read-failed"));
return 1;
}
}
}
int main(int argc, char** argv) { ctron_statics_init(); if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; } { int ctron_ai; for (ctron_ai = 3; ctron_ai < argc; ctron_ai++) { if (!strncmp(argv[ctron_ai], "--format=", 9)) { ctron_cli_fmt = argv[ctron_ai] + 9; } else if (!strncmp(argv[ctron_ai], "--profile=", 10)) { ctron_cli_prof = argv[ctron_ai] + 10; } else if (!strcmp(argv[ctron_ai], "--trusted")) { ctron_cli_trusted = "1"; } else if (!strcmp(argv[ctron_ai], "--dump-gui")) { ctron_cli_dumpgui = "1"; } } } 
#if defined(_WIN32) 
SetConsoleOutputCP(CP_UTF8); 
_setmode(_fileno(stdout), _O_BINARY); 
#endif
 if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; } return (int)t_main(); }
