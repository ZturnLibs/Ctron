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
static void* ct_gc_alloc(size_t n);
static long long ct_gc_live_bytes(void);
static long long ct_gc_collect(void);
static void ct_mcreg_add(void* p, size_t words);
static int ct_gc_conc = 0;
typedef struct { int32_t v; } ctron_cell;
static ctron_list* ct_ig_l(ctron_list* src, int mode, size_t esz, int imode) { if (!src) return 0; ctron_list* d = (ctron_list*)ct_gc_alloc(sizeof(ctron_list)); d->magic = 0x4354726F6E4C7374ULL; d->n = src->n; d->cap = src->n > 0 ? src->n : 0; d->items = 0; if (src->n > 0 && src->items) { d->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)src->n); for (int q = 0; q < src->n; q++) { char* v = src->items[q]; char* w = v; if (mode == 0) { w = v; } else if (mode == 1) { size_t n = v ? strlen(v) + 1 : 1; char* r2 = (char*)ct_gc_alloc(n); if (v) memcpy(r2, v, n); r2[n-1] = 0; w = r2; } else if (mode == 2) { w = (char*)ct_ig_l((ctron_list*)v, imode, esz, -1); } else { void* b2 = ct_gc_alloc(esz); if (v) memcpy(b2, v, esz); w = (char*)b2; } d->items[q] = w; } } return d; }
static ctron_list* ctron_list_new(void) { ctron_list* l = (ctron_list*)ct_gc_alloc(sizeof(ctron_list)); l->magic = 0x4354726F6E4C7374ULL; l->items = 0; l->n = 0; l->cap = 0; return l; }
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
typedef struct { void* data; void* vtable; } ct_obj;
static void* deref_ptr(void* p) { return p; }
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
static char* ct_gc_stack_base = 0;
static int ct_gc_on = -1;
static size_t ct_gc_since = 0, ct_gc_thresh = 8u*1024u*1024u, ct_gc_livesz = 0;
typedef struct ct_sb { struct ct_sb* nx; char* cur; char* end; void* fl; size_t cap; size_t live; } ct_sb;
static ct_sb* ct_gc_sbs = 0;
typedef struct ct_mce { void* p; size_t words; struct ct_mce* nx; } ct_mce;
static ct_mce* ct_mcreg_h = 0;
static void ct_mcreg_add(void* p, size_t words) { ct_mce* e = (ct_mce*)malloc(sizeof(ct_mce)); if (!e) return; e->p = p; e->words = words; e->nx = ct_mcreg_h; ct_mcreg_h = e; }
static int ct_gc_enabled(void) { if (ct_gc_on < 0) { const char* g = getenv("CTRON_GC"); ct_gc_on = (g && (!strcmp(g, "1") || !strcmp(g, "on"))) ? 1 : 0; const char* th = getenv("CTRON_GC_THRESHOLD"); if (th && *th) { long v = atol(th); if (v > 0) ct_gc_thresh = (size_t)v; } } return ct_gc_on; }
static int ct_gc_is_gc_ptr(void* w) { for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { char* b = (char*)(s + 1); if ((char*)w >= b && (char*)w < s->end) return 1; } return 0; }
static void ct_gc_mkrange(char* lo, char* hi);
static void ct_gc_mkword(void* w) { if (!ct_gc_is_gc_ptr(w)) return; ct_sb* s = 0; for (ct_sb* q = ct_gc_sbs; q; q = q->nx) { char* b = (char*)(q + 1); if ((char*)w >= b && (char*)w < q->end) { s = q; break; } } size_t* hdr = (size_t*)w; if (hdr[1] != 1 && hdr[1] != 2) return; if (hdr[1] == 2) return; hdr[1] = 2; ct_gc_livesz += hdr[0]; ct_gc_mkrange((char*)w, (char*)w + hdr[0]); unsigned long long mg = 0; if (hdr[0] >= 24) { memcpy(&mg, w, 8); if (mg == 0x4354726F6E4C7374ULL) { char** it = *(char***)((char*)w + 8); int nn = *(int*)((char*)w + 16); if (it && nn > 0) ct_gc_mkrange((char*)it, (char*)it + (size_t)nn * sizeof(char*)); } } }
static void ct_gc_mkrange(char* lo, char* hi) { for (char* w = lo; w + sizeof(void*) <= hi; w += sizeof(void*)) { void* c; memcpy(&c, w, sizeof(void*)); ct_gc_mkword(c); } }
static void* ct_gc_alloc(size_t n) { if (ct_gc_conc || !ct_gc_enabled()) { return ctron_amalloc(n); } if (ct_gc_since > ct_gc_thresh) { ct_gc_collect(); } size_t sz = (n + 15) & ~(size_t)15; for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { void** f = (void**)&s->fl; while (*f) { size_t* h = (size_t*)*f; if (h[0] >= sz) { *f = (void*)h[2]; h[1] = 1; s->live += 1; return (void*)(h + 2); } f = (void**)h + 2; } } size_t cap = 1024u * 1024u; if (cap < sz + 4096) cap = sz + 4096; ct_sb* s = (ct_sb*)malloc(sizeof(ct_sb) + cap); if (!s) { return ctron_amalloc(n); } s->nx = ct_gc_sbs; ct_gc_sbs = s; s->cap = cap; s->fl = 0; s->live = 1; s->cur = (char*)(s + 1); s->end = s->cur + cap; size_t* h = (size_t*)s->cur; h[0] = sz; h[1] = 1; h[2] = 0; s->cur = (char*)s->cur + 16 + sz; ct_gc_since += sz; return (void*)(h + 2); }
static long long ct_gc_collect(void) { if (!ct_gc_enabled()) return -1; ct_gc_livesz = 0; char* fa = (char*)__builtin_frame_address(0); if (ct_gc_stack_base && fa < ct_gc_stack_base) ct_gc_mkrange(fa, ct_gc_stack_base); for (int i = 0; i < ct_nchans; i++) { if (ct_chans[i] && ct_chans[i]->buf && ct_chans[i]->cap > 0) ct_gc_mkrange((char*)ct_chans[i]->buf, (char*)ct_chans[i]->buf + (size_t)ct_chans[i]->cap * sizeof(ct_i)); } for (ct_mce* e = ct_mcreg_h; e; e = e->nx) { ct_gc_mkrange((char*)e->p, (char*)e->p + e->words * sizeof(void*)); } if (ctron_abase) ct_gc_mkrange(ctron_abase, ctron_abase + ctron_aoff); for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { char* p = (char*)(s + 1); while (p + 16 <= s->cur) { size_t* h = (size_t*)p; if (h[1] == 2) { h[1] = 1; p += 16 + h[0]; } else if (h[1] == 1) { h[1] = 0; s->live -= 1; *(void**)(h + 2) = s->fl; s->fl = (void*)h; p += 16 + h[0]; } else { p += 16 + h[0]; } } } ct_gc_since = 0; size_t keep = 0; ct_sb** pp = &ct_gc_sbs; while (*pp) { ct_sb* s = *pp; if (s->live == 0) { *pp = s->nx; free(s); } else { keep += s->live; pp = &s->nx; } } return (long long)ct_gc_livesz; }
static long long ct_gc_live_bytes(void) { return ct_gc_enabled() ? (long long)ct_gc_livesz : -1LL; }
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
static ct_task* ct_spawn(void* (*shim)(void*), void* env, ct_scope* sc) { ct_gc_conc = 1; ct_task* t = (ct_task*)calloc(1, sizeof(ct_task)); t->shim = shim; t->env = env; t->scope = sc; if (!ct_rt_active() && ct_rt_env_coro()) ctron_rt_init(0); if (ct_rt_active()) { ctron_rt_run((void (*)(void*))ct_shim_tramp, t, (void*)t); return t; } pthread_create(&t->th, 0, ct_shim_tramp, t); return t; }
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
typedef struct { ctron_list* src; ctron_list* dst; ct_fn1 f; int lo, hi; } ct_pmap_arg;
static void* ct_pmap_work(void* p) { ct_pmap_arg* a = (ct_pmap_arg*)p; for (int i = a->lo; i < a->hi; i++) a->dst->items[i] = (char*)(long)a->f((ct_i)(long)a->src->items[i]); return 0; }
typedef struct { void* base; int w64; ctron_list* dst; ct_fn1 f; int lo, hi; } ct_pmap_arga;
static void* ct_pmap_work_a(void* p) { ct_pmap_arga* a = (ct_pmap_arga*)p; for (int i = a->lo; i < a->hi; i++) { ct_i v = a->w64 ? ((int64_t*)a->base)[i] : (ct_i)((int32_t*)a->base)[i]; a->dst->items[i] = (char*)(long)a->f(v); } return 0; }
typedef struct { void* base; int w64; ct_fn2 f; ct_i init; int lo, hi; ct_i out; } ct_pred_arga;
static void* ct_pred_work_a(void* p) { ct_pred_arga* a = (ct_pred_arga*)p; ct_i acc = a->init; for (int i = a->lo; i < a->hi; i++) { ct_i v = a->w64 ? ((int64_t*)a->base)[i] : (ct_i)((int32_t*)a->base)[i]; acc = a->f(acc, v); } a->out = acc; return 0; }
static ctron_list* ct_pmap_par(ctron_list* a, ct_fn1 f) { ctron_list* r = ctron_list_new(); if (a->n <= 0) return r; r->items = (char**)ctron_amalloc(sizeof(char*) * (size_t)a->n); r->cap = a->n; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pmap_arg ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > a->n) ag[k].hi = a->n; if (ag[k].lo >= ag[k].hi) { ag[k].hi = ag[k].lo; } pthread_create(&th[k], 0, ct_pmap_work, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = a->n; return r; }
typedef struct { ctron_list* src; ct_fn2 f; ct_i init; int lo, hi; ct_i out; } ct_pred_arg;
static void* ct_pred_work(void* p) { ct_pred_arg* a = (ct_pred_arg*)p; ct_i acc = a->init; for (int i = a->lo; i < a->hi; i++) acc = a->f(acc, (ct_i)(long)a->src->items[i]); a->out = acc; return 0; }
static ct_i ct_pred_par(ctron_list* a, ct_i init, ct_fn2 f) { if (a->n <= 0) return init; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pred_arg ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > a->n) ag[k].hi = a->n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; pthread_create(&th[k], 0, ct_pred_work, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; int first = 1; for (int k = 0; k < K; k++) { if (ag[k].lo >= ag[k].hi) continue; if (first) { acc = ag[k].out; first = 0; } else { acc = f(acc, ag[k].out); } } return acc; }
static ctron_list* ct_pmap_par_a(void* base, int n, int w64, ct_fn1 f) { ctron_list* r = ctron_list_new(); if (n <= 0) return r; r->items = (char**)ctron_amalloc(sizeof(char*) * (size_t)n); r->cap = n; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pmap_arga ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; if (ag[k].lo >= ag[k].hi) ag[k].hi = ag[k].lo; pthread_create(&th[k], 0, ct_pmap_work_a, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = n; return r; }
static ct_i ct_pred_par_a(void* base, int n, int w64, ct_fn2 f, ct_i init) { if (n <= 0) return init; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pred_arga ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; pthread_create(&th[k], 0, ct_pred_work_a, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; int first = 1; for (int k = 0; k < K; k++) { if (ag[k].lo >= ag[k].hi) continue; if (first) { acc = ag[k].out; first = 0; } else { acc = f(acc, ag[k].out); } } return acc; }
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
static const char* ctron_version = "v0.0.1-707-g6d28b4c";
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
static int ctron_argc_g = 0;
static char** ctron_argv_g = 0;
static int64_t ctron_prog_args_n(void) { return (int64_t)ctron_argc_g; }
static const char* ctron_prog_args_at(int64_t i) { return (i >= 0 && i < (int64_t)ctron_argc_g) ? ctron_argv_g[i] : ""; }
typedef struct { int32_t* d; int64_t n; } ctron_view_i;
typedef struct { int64_t* d; int64_t n; } ctron_view_6;
typedef struct { int* d; int64_t n; } ctron_view_b;
typedef struct { const char** d; int64_t n; } ctron_view_s;
typedef struct { double* d; int64_t n; } ctron_view_f;
typedef struct { float* d; int64_t n; } ctron_view_g;
typedef struct { uint64_t* d; int64_t n; } ctron_view_7;
typedef struct { uint8_t* d; int64_t n; } ctron_view_w8u;
typedef struct { int8_t* d; int64_t n; } ctron_view_w8s;
typedef struct { uint16_t* d; int64_t n; } ctron_view_w16u;
typedef struct { int16_t* d; int64_t n; } ctron_view_w16s;
typedef struct { size_t* d; int64_t n; } ctron_view_z;
static int32_t t_main();
int32_t t_main() 
{
printf("%s\n", (const char*)("t36-ok"));
return 0;
}
int main(int argc, char** argv) { ctron_argc_g = argc; ctron_argv_g = argv; ct_gc_stack_base = (char*)&argc; if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; } { int ctron_ai; for (ctron_ai = 3; ctron_ai < argc; ctron_ai++) { if (!strncmp(argv[ctron_ai], "--format=", 9)) { ctron_cli_fmt = argv[ctron_ai] + 9; } else if (!strncmp(argv[ctron_ai], "--profile=", 10)) { ctron_cli_prof = argv[ctron_ai] + 10; } else if (!strcmp(argv[ctron_ai], "--trusted")) { ctron_cli_trusted = "1"; } else if (!strcmp(argv[ctron_ai], "--dump-gui")) { ctron_cli_dumpgui = "1"; } } } 
#if defined(_WIN32) 
SetConsoleOutputCP(CP_UTF8); 
_setmode(_fileno(stdout), _O_BINARY); 
#endif
 if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; } return (int)t_main(); }
