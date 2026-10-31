#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <pthread.h>
#include <setjmp.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
static int ctron_w_setenv(const char* k9, const char* v9) { int n9 = strlen(k9) + strlen(v9) + 2; char* b9 = (char*)malloc((size_t)n9); if (!b9) { return -1; } snprintf(b9, (size_t)n9, "%s=%s", k9, v9); int r9 = _putenv(b9); free(b9); return r9; }
#define setenv(k, v, o) ctron_w_setenv(k, v)
#define mkdir(p, m) _mkdir(p)
#define RTLD_NOW 0
#define RTLD_GLOBAL 0
#define RTLD_DEFAULT ((void*)0)
static void* ctron_w_dlopen(const char* p7, int m7) { int w7 = MultiByteToWideChar(CP_UTF8, 0, p7, -1, 0, 0); wchar_t* q7 = 0; if (w7 > 0) { q7 = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)w7); MultiByteToWideChar(CP_UTF8, 0, p7, -1, q7, w7); } (void)m7; void* r7 = q7 ? (void*)LoadLibraryW(q7) : (void*)0; free(q7); return r7; }
static void* ctron_w_dlsym(void* h7, const char* s7) { if (!h7) { h7 = (void*)GetModuleHandleW(0); } return (void*)GetProcAddress((HMODULE)h7, s7); }
static int ctron_w_dlclose(void* h7) { return FreeLibrary((HMODULE)h7) ? 0 : -1; }
static char ctron_w_dlebuf[256];
static const char* ctron_w_dlerror(void) { DWORD e7 = GetLastError(); if (!e7) { return 0; } SetLastError(0); wchar_t v7[128]; DWORD n7 = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, e7, 0, v7, 128, 0); if (!n7) { return 0; } int r7 = WideCharToMultiByte(CP_UTF8, 0, v7, (int)n7, ctron_w_dlebuf, 255, 0, 0); if (r7 <= 0) { return 0; } ctron_w_dlebuf[r7] = 0; while (r7 > 0 && (ctron_w_dlebuf[r7-1] == 10 || ctron_w_dlebuf[r7-1] == 13 || ctron_w_dlebuf[r7-1] == 32)) { ctron_w_dlebuf[--r7] = 0; } return ctron_w_dlebuf; }
#define dlopen ctron_w_dlopen
#define dlsym ctron_w_dlsym
#define dlclose ctron_w_dlclose
#define dlerror ctron_w_dlerror
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
static int ctron_alock = 0;
typedef struct ct_ablk { struct ct_ablk* nx; size_t cap; size_t used; } ct_ablk;
typedef struct ct_arn { ct_ablk* cur; size_t off; } ct_arn;
static ct_arn ctron_aglobal;
static __thread ct_arn* ct_tls_arn = 0;
static void ctron_panic(const char* m);
static void* ct_ablk_new(size_t cap) { ct_ablk* b = (ct_ablk*)malloc(sizeof(ct_ablk) + cap); if (!b) { ctron_panic("arena OOM"); } b->nx = 0; b->cap = cap; b->used = 0; return b; }
static void* ctron_amalloc(size_t n) { n = (n + 15) & ~(size_t)15; ct_arn* a = ct_tls_arn ? ct_tls_arn : &ctron_aglobal; int lk = (a == &ctron_aglobal); if (lk) { while (__sync_lock_test_and_set(&ctron_alock, 1)) { } } if (!a->cur || a->off + n > a->cur->cap) { size_t nc = a->cur ? a->cur->cap * 2 : (size_t)16384; while (nc < n) { nc = nc * 2; } ct_ablk* nb = (ct_ablk*)malloc(sizeof(ct_ablk) + nc); if (!nb) { if (lk) { __sync_lock_release(&ctron_alock); } ctron_panic("arena OOM"); } if (a->cur) { a->cur->used = a->off; } nb->nx = a->cur; nb->cap = nc; nb->used = 0; a->cur = nb; a->off = 0; } void* p = (char*)(a->cur + 1) + a->off; a->off += n; if (lk) { __sync_lock_release(&ctron_alock); } return p; }
static int64_t ctron_str_to_i64(const char* s) { int64_t r = 0; int neg = 0; long k = 0; if (s[k] == 45) { neg = 1; k++; } else if (s[k] == 43) { k++; } while (s[k] >= 48 && s[k] <= 57) { r = r * 10 + (s[k] - 48); k++; } return neg ? -r : r; }
typedef struct { unsigned long long magic; char** items; int n; int cap; } ctron_list;
static void* ct_gc_alloc(size_t n);
static long long ct_gc_live_bytes(void);
static long long ct_gc_collect(void);
static void ct_mcreg_add(void* p, size_t words);
static int ct_gc_conc = 0;
typedef struct { int32_t v; } ctron_cell;
static ctron_list* ct_ig_l(ctron_list* src, int mode, size_t esz, int imode) { if (!src) return 0; ctron_list* d = (ctron_list*)ct_gc_alloc(sizeof(ctron_list)); d->magic = 0x4354726F6E4C7374ULL; d->n = src->n; d->cap = src->n > 0 ? src->n : 0; d->items = 0; if (src->n > 0 && src->items) { d->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)src->n); for (int q = 0; q < src->n; q++) { char* v = src->items[q]; char* w = v; if (mode == 0) { w = v; } else if (mode == 1) { size_t n = v ? strlen(v) + 1 : 1; char* r2 = (char*)ct_gc_alloc(n); if (v) memcpy(r2, v, n); r2[n-1] = 0; w = r2; } else if (mode == 2) { w = (char*)ct_ig_l((ctron_list*)v, imode, esz, -1); } else { void* b2 = ct_gc_alloc(esz); if (v) memcpy(b2, v, esz); w = (char*)b2; } d->items[q] = w; } } return d; }
static ctron_list* ctron_list_new(void) { ctron_list* l = (ctron_list*)ct_gc_alloc(sizeof(ctron_list)); l->magic = 0x4354726F6E4C7374ULL; l->items = 0; l->n = 0; l->cap = 0; return l; }
typedef struct { unsigned long long magic; char* data; int len; int cap; } ctron_sb;
static char* ctron_sb_new(void) { ctron_sb* sb = (ctron_sb*)ct_gc_alloc(sizeof(ctron_sb)); sb->magic = 0x4374726F6E534231ULL; sb->data = (char*)ct_gc_alloc(64); sb->data[0] = 0; sb->len = 0; sb->cap = 64; return (char*)sb; }
static void ctron_sb_push(const void* h, const char* s) { ctron_sb* sb = (ctron_sb*)h; int n = (int)strlen(s); if (sb->len + n + 1 > sb->cap) { int nc = sb->cap ? sb->cap * 2 : 64; while (nc < sb->len + n + 1) { nc = nc * 2; } char* r = (char*)ct_gc_alloc((size_t)nc); memcpy(r, sb->data, (size_t)sb->len); sb->data = r; sb->cap = nc; } memcpy(sb->data + sb->len, s, (size_t)n); sb->len += n; sb->data[sb->len] = 0; }
static void ctron_sb_reserve(const void* h, int n) { ctron_sb* sb = (ctron_sb*)h; if (n <= 0) { return; } if (sb->len + n + 1 > sb->cap) { int nc = sb->cap ? sb->cap * 2 : 64; while (nc < sb->len + n + 1) { nc = nc * 2; } char* r = (char*)ct_gc_alloc((size_t)nc); memcpy(r, sb->data, (size_t)sb->len); sb->data = r; sb->cap = nc; } }
static void ctron_sb_clear(const void* h) { ctron_sb* sb = (ctron_sb*)h; sb->len = 0; sb->data[0] = 0; }
static const char* ctron_sb_str(const void* p) { const char* s0 = (const char*)p; unsigned long long m2 = 0; if (strlen(s0) >= 8) { memcpy(&m2, s0, 8); if (m2 == 0x4374726F6E534231ULL) { const ctron_sb* sb2 = (const ctron_sb*)p; size_t dn = (size_t)sb2->len; char* r = (char*)ct_gc_alloc(dn + 1); memcpy(r, sb2->data, dn + 1); return r; } } return s0; }
static int ctron_len(const void* p) { const char* s = (const char*)p; unsigned long long m = 0; if (strlen(s) >= 8) { memcpy(&m, s, 8); if (m == 0x4354726F6E4C7374ULL) { return ((const ctron_list*)p)->n; } if (m == 0x4374726F6E534231ULL) { return (int)strlen(((const ctron_sb*)p)->data); } } return (int)strlen(s); }
static void ctron_list_push(ctron_list* l, const char* s) { if (l->n == l->cap) { int nc = l->cap ? l->cap * 2 : 8; char** ni = (char**)ct_gc_alloc(sizeof(char*) * (size_t)nc); int q = 0; while (q < l->n) { ni[q] = l->items[q]; q += 1; } l->items = ni; l->cap = nc; } l->items[l->n] = (char*)s; l->n += 1; }
static ctron_cell* ctron_cell_new(int32_t v0) { ctron_cell* c = (ctron_cell*)ct_gc_alloc(sizeof(ctron_cell)); c->v = v0; return c; }
static void ctron_print_i32(int32_t v) { printf("%d\n", v); }
static const char* ctron_str_concat(const char* a, const char* b) { size_t la = strlen(a), lb = strlen(b); char* r = (char*)ct_gc_alloc(la + lb + 1); memcpy(r, a, la); memcpy(r + la, b, lb); r[la + lb] = 0; return r; }
static const char* ctron_i32_to_string(int32_t v) { char* r = (char*)ct_gc_alloc(16); snprintf(r, 16, "%d", v); return r; }
static void ctron_print_i64(int64_t v) { printf("%lld\n", (long long)v); }
static int ctron_byte_at(const char* s, int i) { return (int)(unsigned char)s[i]; }
static const char* ctron_str_byte(int b) { char* r = (char*)ct_gc_alloc(2); r[0] = (char)(b & 0xFF); r[1] = 0; return r; }
static const char* ctron_utf8_enc(int cp) { static const char* rep = "\xEF\xBF\xBD"; unsigned char b[5]; int un = 0; if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { return rep; } if (cp < 0x80) { b[un++] = (unsigned char)cp; } else if (cp < 0x800) { b[un++] = (unsigned char)(0xC0 | (cp >> 6)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } else if (cp < 0x10000) { b[un++] = (unsigned char)(0xE0 | (cp >> 12)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } else { b[un++] = (unsigned char)(0xF0 | (cp >> 18)); b[un++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F)); b[un++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F)); b[un++] = (unsigned char)(0x80 | (cp & 0x3F)); } b[un] = 0; char* r = (char*)ct_gc_alloc((size_t)un + 1); memcpy(r, b, (size_t)un); r[un] = 0; return r; }
static const char* ctron_str_byte_t(const char* dec) { return ctron_str_byte((int)strtol(dec, 0, 10)); }
static const char* ctron_utf8_enc_t(const char* dec) { return ctron_utf8_enc((int)strtol(dec, 0, 10)); }
#define CTB_M(bits) (bits == 64 ? ~(unsigned long long)0 : ((1ULL << bits) - 1))
#include <time.h>
static long long ctron_clock_now_ns(void) { static long long base = -1; struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); long long ns = (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec; if (base < 0) { base = ns; } return ns - base; }
static long long ctron_bit_i64(int op, long long a, long long b, int bits, int sgn) { return (bits == 32) ? (long long)(int32_t)(uint32_t)(op == 0 ? (a & b) : op == 1 ? (a | b) : op == 2 ? (a ^ b) : op == 3 ? (a ^ CTB_M(bits)) : op == 4 ? ((a << b) & CTB_M(bits)) : sgn ? (((bits == 32 ? (long long)(int32_t)a : a) >> b) & CTB_M(bits)) : (a >> b)) : (long long)(op == 0 ? (a & b) : op == 1 ? (a | b) : op == 2 ? (a ^ b) : op == 3 ? (a ^ CTB_M(bits)) : op == 4 ? ((a << b) & CTB_M(bits)) : sgn ? (((bits == 32 ? (long long)(int32_t)a : a) >> b) & CTB_M(bits)) : (a >> b)); }
static void ctron_panic(const char* m);
static const char* ctron_byte_slice(const char* s, int a, int b) { int n = b - a; if (n < 0 || a < 0 || (long)a + (long)n > (long)strlen(s)) { ctron_panic("byte_slice: range out of bounds"); } char* r = (char*)ct_gc_alloc((size_t)n + 1); memcpy(r, s + a, (size_t)n); r[n] = 0; return r; }
static const char* ctron_str_from_c(const char* p) { size_t n = strlen(p); char* r = (char*)ct_gc_alloc(n + 1); memcpy(r, p, n + 1); return r; }
static const char* ctron_dlerror_str(void) { const char* e5 = dlerror(); if (!e5) { return ""; } size_t n5 = strlen(e5); char* r5 = (char*)ct_gc_alloc(n5 + 1); memcpy(r5, e5, n5 + 1); return r5; }
static const char* ctron_read_file(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return ""; } fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); char* r = (char*)ct_gc_alloc((size_t)n + 1); size_t rd = fread(r, 1, (size_t)n, f); r[rd] = 0; fclose(f); return r; }
static const char* ctron_read_line(void) { size_t cap = 256; size_t n = 0; char* buf = (char*)malloc(cap); if (!buf) { abort(); } int c; while ((c = fgetc(stdin)) != EOF) { if (c == 10) { break; } if (n + 2 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); if (!buf) { abort(); } } buf[n] = (char)c; n++; } if (c == EOF && n == 0) { free(buf); return ""; } if (n > 0 && buf[n - 1] == 13) { n--; } buf[n] = 0; char* r = (char*)ct_gc_alloc(n + 1); memcpy(r, buf, n + 1); free(buf); return r; }
static const char* ctron_read_bytes(int64_t want) { if (want < 0) { want = 0; } char* r = (char*)ct_gc_alloc((size_t)want + 1); size_t got = 0; while (got < (size_t)want) { size_t k = fread(r + got, 1, (size_t)want - got, stdin); if (k == 0) { break; } got += k; } r[got] = 0; return r; }
static const char* ctron_read_dir(const char* path) { DIR* d = opendir(path); if (!d) { return NULL; } size_t cap = 256; size_t len = 0; char* r = (char*)malloc(cap); r[0] = 0; struct dirent* de; while ((de = readdir(d)) != NULL) { if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) { continue; } size_t bl = strlen(de->d_name); while (len + bl + 2 > cap) { cap *= 2; } char* nb = (char*)realloc(r, cap); r = nb; memcpy(r + len, de->d_name, bl); len += bl; r[len] = 10; len += 1; } closedir(d); r[len] = 0; return r; }
static int ctron_fs_exists(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return 0; } fclose(f); return 1; }
static int ctron_fs_write(const char* path, const char* data) { FILE* f = fopen(path, "wb"); if (!f) { return 0; } size_t n = strlen(data); size_t w = fwrite(data, 1, n, f); fclose(f); return w == n; }
static int ctron_fs_mkdir(const char* path) {
#include <sys/stat.h>
#include <errno.h>
 char tmp[4096]; size_t len = strlen(path); if (len == 0 || len >= sizeof(tmp)) { return 0; } memcpy(tmp, path, len + 1); for (char* p = tmp + 1; *p; p++) { if (*p == '/') { *p = 0; if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { return 0; } *p = '/'; } } if (mkdir(tmp, 0755) != 0 && errno != EEXIST) { return 0; } return 1; }
int32_t ctron_fs_mkdir_p(const char* path) { return (int32_t)ctron_fs_mkdir(path); }
static int ctron_fs_delete(const char* path) { return remove(path) == 0; }
static void ctron_sleep_ms(long long ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L; nanosleep(&ts, 0); }
static void ctron_sleep_ns(long long ns) { struct timespec ts; ts.tv_sec = ns / 1000000000LL; ts.tv_nsec = ns % 1000000000LL; nanosleep(&ts, 0); }
static int64_t ctron_now_ms(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL; }
static const char* ctron_now_ms_text(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); long long ms = (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL; char* r = (char*)ct_gc_alloc(32); snprintf(r, 32, "%lld", ms); return r; }
static int64_t ctron_mono_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static const char* ctron_mono_ns_text(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); long long ns = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec; char* r = (char*)ct_gc_alloc(32); snprintf(r, 32, "%lld", ns); return r; }
static int64_t ctron_wall_ns(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec; }
static const char* ctron_wall_ns_text(void) { struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); long long ns = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec; char* r = (char*)ct_gc_alloc(32); snprintf(r, 32, "%lld", ns); return r; }
typedef int64_t ct_i;
typedef struct { int variant; ct_i v; } ct_res;
typedef struct { void* data; void* vtable; } ct_obj;
static void* deref_ptr(void* p) { return p; }
typedef struct ct_scope { int cancelled; pthread_mutex_t mu; struct ct_task* head; struct ct_task* tail; } ct_scope;
static ct_scope* ct_scope_new(void) { ct_scope* s = (ct_scope*)calloc(1, sizeof(ct_scope)); pthread_mutex_init(&s->mu, 0); return s; }
static pthread_mutex_t ct_gm = PTHREAD_MUTEX_INITIALIZER;
static void ct_glock(void) { pthread_mutex_lock(&ct_gm); }
static void ct_gunlock(void) { pthread_mutex_unlock(&ct_gm); }
typedef struct ct_task { pthread_t th; void* env; void* (*shim)(void*); ct_i result; int state; char pmsg[128]; ct_scope* scope; void* arn; int joined; struct ct_task* snext; pthread_mutex_t done_mu; pthread_cond_t done_cv; int done; jmp_buf jmp; } ct_task;
static __thread ct_task* ct_tls_task = 0;
__attribute__((noinline)) static ct_task** ct_tls_slot(void) { return &ct_tls_task; }
typedef struct ct_chan { pthread_mutex_t mu; pthread_cond_t cv_full, cv_empty; ct_i* buf; int head, tail, cnt, cap; int is_str; } ct_chan;
static ct_chan** ct_chans = 0; static int ct_nchans = 0, ct_chancap = 0; static pthread_mutex_t ct_chreg = PTHREAD_MUTEX_INITIALIZER;
static char* ct_gc_stack_base = 0;
static int ct_gc_on = -1;
static size_t ct_gc_since = 0, ct_gc_thresh = 8u*1024u*1024u, ct_gc_livesz = 0;
typedef struct ct_sb { struct ct_sb* nx; char* cur; char* end; void* fl; size_t cap; size_t live; size_t bsz; int empty_age; } ct_sb;
static ct_sb* ct_gc_sbs = 0;
static char** ct_gc_wl = 0;
static size_t ct_gc_wln = 0, ct_gc_wlc = 0;
static void ctron_panic(const char* m);
static void ct_gc_wlput(char* lo, char* hi) { if (ct_gc_wln + 2 > ct_gc_wlc) { size_t nc = ct_gc_wlc ? ct_gc_wlc * 2 : 4096; char** nw = (char**)realloc(ct_gc_wl, nc * sizeof(char*)); if (!nw) { ctron_panic("gc: worklist OOM(realloc败=标记候选丢失=静默UAF,响亮退出)"); } ct_gc_wl = nw; ct_gc_wlc = nc; } ct_gc_wl[ct_gc_wln++] = lo; ct_gc_wl[ct_gc_wln++] = hi; }
typedef struct ct_frs { void** addr; int kind; size_t sz; } ct_frs;
typedef struct ct_fr { struct ct_fr* up; ct_frs pool[128]; int pn; int ovf; int sv[32]; int spn; } ct_fr;
static __thread ct_fr* ct_fp = 0;
static void ct_fp_pop(void* p) { ct_fp = ((ct_fr*)p)->up; }
static int ct_gc_conserv = 0;
static int ct_gc_arena_scan = 1;
static int ct_gc_ovf = 0;
typedef struct ct_mce { void* p; size_t words; struct ct_mce* nx; } ct_mce;
static ct_mce* ct_mcreg_h = 0;
static void ct_mcreg_add(void* p, size_t words) { ct_mce* e = (ct_mce*)malloc(sizeof(ct_mce)); if (!e) return; e->p = p; e->words = words; e->nx = ct_mcreg_h; ct_mcreg_h = e; }
static int ct_gc_enabled(void) { if (ct_gc_on < 0) { const char* g = getenv("CTRON_GC"); ct_gc_on = (g && (!strcmp(g, "1") || !strcmp(g, "on"))) ? 1 : 0; const char* th = getenv("CTRON_GC_THRESHOLD"); if (th && *th) { long v = atol(th); if (v > 0) ct_gc_thresh = (size_t)v; } const char* cv = getenv("CTRON_GC_CONSERV"); ct_gc_conserv = 1; if (cv && (*cv == '0' || *cv == 'f')) { ct_gc_conserv = 0; } const char* av = getenv("CTRON_GC_ARENASCAN"); ct_gc_arena_scan = 1; if (av && (*av == '0' || *av == 'f')) { ct_gc_arena_scan = 0; } } return ct_gc_on; } // T31 GC 默认关(v1「解释器堆不回收」契约;CTRON_GC=1 选入)——精确收集的根集扫描对解释器深帧/跨测试持有存在洞,主线容器值被回收即 UAF(2026-10-31 容器三伤根因)
static void ct_gc_mkword(void* w) { ct_sb* s = 0; for (ct_sb* q = ct_gc_sbs; q; q = q->nx) { char* b = (char*)(q + 1); if ((char*)w >= b && (char*)w < q->end) { s = q; break; } } if (!s) { return; } size_t st = 16 + s->bsz; size_t off = (size_t)((char*)w - (char*)(s + 1)); size_t* hdr = (size_t*)((char*)(s + 1) + (off / st) * st); if (hdr[1] != 1) { return; } if (hdr[0] == 0 || hdr[0] > s->bsz) { return; } hdr[1] = 2; ct_gc_livesz += hdr[0]; ct_gc_wlput((char*)(hdr + 2), (char*)(hdr + 2) + hdr[0]); if (hdr[0] >= 24) { unsigned long long mg; memcpy(&mg, hdr + 2, 8); if (mg == 0x4354726F6E4C7374ULL) { char** it = *(char***)((char*)(hdr + 2) + 8); int nn = *(int*)((char*)(hdr + 2) + 16); if (it && nn > 0) { ct_gc_wlput((char*)it, (char*)it + (size_t)nn * sizeof(char*)); } } } }
static void* ct_gc_alloc(size_t n) { if (ct_gc_conc || !ct_gc_enabled() || ct_tls_arn) { return ctron_amalloc(n); } if (ct_gc_since > ct_gc_thresh && ct_gc_since > ct_gc_livesz) { ct_gc_collect(); } /* S4④:堆自适应——churn 超过活集才收集,摊销全堆标记 <=2x;8MB 底线保小堆响应 */ size_t sz = (n + 15) & ~(size_t)15; ct_sb* best = 0; for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { if (s->bsz >= sz && s->fl) { if (!best || s->bsz < best->bsz) { best = s; } } } if (best) { size_t* h = (size_t*)best->fl; best->fl = (void*)h[2]; h[0] = sz; h[1] = 1; best->live += 1; ct_gc_since += sz; return (void*)(h + 2); } ct_sb* carve = 0; for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { if (s->bsz >= sz && s->cur + 16 + s->bsz <= s->end) { if (!carve || s->bsz < carve->bsz) { carve = s; } } } size_t bsz = 32; while (bsz < sz) { bsz *= 2; } if (!carve) { size_t st = 16 + bsz; size_t cap = 1024u * 1024u; if (cap < st) { cap = st; } cap = (cap / st) * st; ct_sb* s = (ct_sb*)calloc(1, sizeof(ct_sb) + cap); if (!s) { return ct_gc_alloc(n); } s->nx = ct_gc_sbs; ct_gc_sbs = s; s->cap = cap; s->fl = 0; s->live = 0; s->bsz = bsz; s->cur = (char*)(s + 1); s->end = s->cur + cap; carve = s; } size_t st = 16 + carve->bsz; size_t* h = (size_t*)carve->cur; h[0] = sz; h[1] = 1; h[2] = 0; carve->cur = carve->cur + st; carve->live += 1; ct_gc_since += sz; return (void*)(h + 2); }
static long long ct_gc_collect_inner(void);
// Boehm GC_with_callee_saves 同款壳:setjmp 必须先于一切可打脏调用方

// 寄存器的语句——薄壳捕获全寄存器态落 jmp_buf 入根集,实体下沉 inner

static long long ct_gc_collect(void) { jmp_buf ct_rjb; setjmp(ct_rjb); ct_gc_wlput((char*)ct_rjb, (char*)ct_rjb + sizeof(ct_rjb)); return ct_gc_collect_inner(); }
static long long ct_gc_collect_inner(void) { __asm__ volatile("" : : : "memory"); ct_gc_livesz = 0; ct_gc_wln = 0; ct_gc_ovf = 0; jmp_buf ct_rjb; setjmp(ct_rjb); ct_gc_wlput((char*)ct_rjb, (char*)ct_rjb + sizeof(ct_rjb)); for (ct_fr* f = ct_fp; f; f = f->up) { if (f->ovf) { ct_gc_ovf = 1; } for (int pi3 = 0; pi3 < f->pn; pi3++) { ct_frs* s = &f->pool[pi3]; if (s->kind == 0) { ct_gc_mkword(*(void**)s->addr); } else { ct_gc_wlput((char*)s->addr, (char*)s->addr + s->sz); } } } char* fa = (char*)__builtin_frame_address(0); if ((ct_gc_conserv || ct_gc_ovf) && ct_gc_stack_base && fa < ct_gc_stack_base) ct_gc_wlput(fa, ct_gc_stack_base); for (int i = 0; i < ct_nchans; i++) { if (ct_chans[i] && ct_chans[i]->buf && ct_chans[i]->cap > 0) { ct_gc_wlput((char*)ct_chans[i]->buf, (char*)ct_chans[i]->buf + (size_t)ct_chans[i]->cap * sizeof(ct_i)); } } for (ct_mce* e = ct_mcreg_h; e; e = e->nx) { ct_gc_wlput((char*)e->p, (char*)e->p + e->words * sizeof(void*)); } if (ct_gc_arena_scan) { ct_ablk* b3 = ctron_aglobal.cur; if (b3) { ct_gc_wlput((char*)(b3 + 1), (char*)(b3 + 1) + ctron_aglobal.off); } for (b3 = b3 ? b3->nx : (ct_ablk*)0; b3; b3 = b3->nx) { ct_gc_wlput((char*)(b3 + 1), (char*)(b3 + 1) + b3->used); } } while (ct_gc_wln > 0) { char* hi2 = ct_gc_wl[--ct_gc_wln]; char* lo2 = ct_gc_wl[--ct_gc_wln]; for (char* w2 = lo2; w2 + sizeof(void*) <= hi2; w2 += sizeof(void*)) { void* c2; memcpy(&c2, w2, sizeof(void*)); ct_gc_mkword(c2); } } for (ct_sb* s = ct_gc_sbs; s; s = s->nx) { char* b2 = (char*)(s + 1); size_t st2 = 16 + s->bsz; size_t nb2 = (size_t)(s->cur - b2) / st2; for (size_t i2 = 0; i2 < nb2; i2++) { size_t* h = (size_t*)(b2 + i2 * st2); if (h[1] == 2) { h[1] = 1; } else if (h[1] == 1) { h[1] = 0; s->live -= 1; *(void**)(h + 2) = s->fl; s->fl = (void*)h; } } } ct_gc_since = 0; ct_sb** pp = &ct_gc_sbs; while (*pp) { ct_sb* s = *pp; if (s->live == 0) { s->empty_age += 1; if (s->empty_age >= 2) { *pp = s->nx; free(s); } else { pp = &s->nx; } } else { s->empty_age = 0; pp = &s->nx; } } return (long long)ct_gc_livesz; }
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
static void ctron_drop_unwind_to(int n) { while (ct_drn > n) { ct_drn -= 1; ct_drstack[ct_drn].fn(ct_drstack[ct_drn].obj); } }
static void ctron_drop_sentinel(void* p) { (void)p; }
static void ctron_drop_mark(void) { ctron_drop_push(ctron_drop_sentinel, 0); }
static void ctron_drop_unwind_mark(int keep) { while (ct_drn > 0) { ct_drn -= 1; if (ct_drstack[ct_drn].fn == ctron_drop_sentinel) { if (keep) { ct_drn += 1; } return; } ct_drstack[ct_drn].fn(ct_drstack[ct_drn].obj); } }
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
static const char* ctron_i64_to_string(int64_t v) { char* r = (char*)ct_gc_alloc(32); snprintf(r, 32, "%lld", (long long)v); return r; }
static const char* ctron_u64_to_string(uint64_t v) { char* r = (char*)ct_gc_alloc(32); snprintf(r, 32, "%llu", (unsigned long long)v); return r; }
static const char* ctron_f64_to_string(double v) { char* r = (char*)ct_gc_alloc(512); if (v == 0.0) { snprintf(r, 512, "0.0"); } else { double t = (v >= -9.22e18 && v <= 9.22e18) ? (double)(long long)v : v; if (v == t) { snprintf(r, 512, "%.1f", v); } else { snprintf(r, 512, "%g", v); } } return r; }
typedef struct ctron_anyerr ctron_anyerr;
struct ctron_anyerr { unsigned long long magic; char* message; ctron_anyerr* cause; char* trace; char* code; void* payload; char* tname; };
static int ct_is_anyerr(const void* p) { const ctron_anyerr* e = (const ctron_anyerr*)p; return e && e->magic == 0x4354414E59455252ULL; }
static const char* ct_err_fld(long v, const char* f) { ctron_anyerr* e = (ctron_anyerr*)v; if (ct_is_anyerr(e)) { if (f[0] == 109) return e->message ? e->message : ""; if (f[0] == 99 && f[1] == 111) return e->code ? e->code : ""; if (f[0] == 116) return e->trace ? e->trace : ""; } return ""; }
struct ct_enum_gen { int variant; ct_i p[2]; };
static const char* ct_dot_join(const char* a, const char* b) { char* r = (char*)ct_gc_alloc((a ? strlen(a) : 0) + (b ? strlen(b) : 0) + 2); sprintf(r, "%s.%s", a ? a : "", b ? b : ""); return r; }
static long ct_anyerr_wrap_e(void* p, const char* tn, const char** vn, const char* vname) { ctron_anyerr* n = (ctron_anyerr*)ct_gc_alloc(sizeof(ctron_anyerr)); n->magic = 0x4354414E59455252ULL; n->message = 0; n->cause = 0; n->trace = 0; n->payload = p; n->tname = (char*)tn; n->code = 0; if (vname) { n->code = (char*)ct_dot_join(tn, vname); } else if (vn && p) { n->code = (char*)ct_dot_join(tn, vn[((struct ct_enum_gen*)p)->variant]); } return (long)n; }
static long ct_anyerr_wrap_s(const char* s) { ctron_anyerr* n = (ctron_anyerr*)ct_gc_alloc(sizeof(ctron_anyerr)); n->magic = 0x4354414E59455252ULL; n->message = 0; n->cause = 0; n->trace = 0; n->code = (char*)s; n->payload = 0; n->tname = 0; return (long)n; }
static char* ct_trace_join(const char* old, const char* msg) { char* r = (char*)ct_gc_alloc((old ? strlen(old) : 0) + (msg ? strlen(msg) : 0) + 12); if (old && old[0]) { sprintf(r, "%s; ctx:%s", old, msg ? msg : ""); } else { sprintf(r, "ctx:%s", msg ? msg : ""); } return r; }
static ct_res ct_ctxres(ct_res v, const char* msg) { if (v.variant == 0) { return v; } ctron_anyerr* o = (ctron_anyerr*)(long)v.v; ctron_anyerr* n = (ctron_anyerr*)ct_gc_alloc(sizeof(ctron_anyerr)); n->magic = 0x4354414E59455252ULL; n->message = (char*)(msg ? msg : ""); if (ct_is_anyerr(o)) { n->cause = o; n->trace = ct_trace_join(o->trace, msg); n->code = o->code; n->payload = o->payload; n->tname = o->tname; } else { n->cause = 0; n->trace = ct_trace_join(0, msg); n->code = 0; n->payload = (void*)o; n->tname = 0; } ct_res r; r.variant = 1; r.v = (ct_i)(long)n; return r; }
static const char* ct_optstr_unr(ct_res v) { if (v.variant == 0) { return (const char*)(long)v.v; } return 0; }
static void* ct_shim_tramp(void* p) { ct_task* t = (ct_task*)p; if (!ct_rt_active()) { ct_arn* ta = (ct_arn*)calloc(1, sizeof(ct_arn)); ta->cur = (ct_ablk*)ct_ablk_new((size_t)16384); ct_tls_arn = ta; t->arn = ta; } *ct_tls_slot() = t; if (setjmp(t->jmp) == 0) { t->shim(t->env); t->state = 1; } ct_fp = 0; pthread_mutex_lock(&t->done_mu); t->done = 1; pthread_cond_broadcast(&t->done_cv); pthread_mutex_unlock(&t->done_mu); if (ct_rt_active()) ctron_rt_notify_done((void*)t); return 0; }
static ct_task* ct_spawn(void* (*shim)(void*), void* env, ct_scope* sc) { ct_task* t = (ct_task*)calloc(1, sizeof(ct_task)); pthread_mutex_init(&t->done_mu, 0); pthread_cond_init(&t->done_cv, 0); t->shim = shim; t->env = env; t->scope = sc; if (sc) { if (sc->tail) { sc->tail->snext = t; } else { sc->head = t; } sc->tail = t; } if (ct_rt_active() || ct_rt_env_coro()) { ct_gc_conc = 1; } if (!ct_rt_active() && ct_rt_env_coro()) ctron_rt_init(0); if (ct_rt_active()) { ctron_rt_run((void (*)(void*))ct_shim_tramp, t, (void*)t); return t; } pthread_attr_t _sa; pthread_attr_init(&_sa); pthread_attr_setstacksize(&_sa, 8u << 20); pthread_create(&t->th, &_sa, ct_shim_tramp, t); return t; }
static void ct_task_seg_free(ct_task* t) { if (t && t->arn) { ct_arn* a = (ct_arn*)t->arn; ct_ablk* b = a->cur; while (b) { ct_ablk* nx = b->nx; free(b); b = nx; } free(a); t->arn = 0; } }
static ct_i ct_join(ct_task* t) { void* r; if (ct_rt_active()) { ctron_rt_join_key((void*)t); } else { pthread_join(t->th, &r); } t->joined = 1; if (t->state == 2) { ct_task_seg_free(t); ctron_panic(t->pmsg); } ct_i v = t->result; ct_task_seg_free(t); return v; }
static ct_res ct_join_or(ct_task* t) { void* r; if (ct_rt_active()) { ctron_rt_join_key((void*)t); } else { pthread_join(t->th, &r); } t->joined = 1; ct_task_seg_free(t); ct_res res; if (t->state == 2) { res.variant = 1; res.v = (ct_i)(long)"task panic"; } else { res.variant = 0; res.v = t->result; } return res; }
typedef struct ct_pitem { void* (*shim)(void*); void* env; struct ct_pitem* nx; } ct_pitem;
typedef struct ct_tpool { pthread_mutex_t mu; pthread_cond_t cv; ct_pitem* head; ct_pitem* tail; int n; int stop; pthread_t th[16]; } ct_tpool;
static void* ct_pool_worker(void* p) { ct_tpool* tp = (ct_tpool*)p; ct_gc_conc = 1; for (;;) { pthread_mutex_lock(&tp->mu); while (tp->head == 0 && !tp->stop) { pthread_cond_wait(&tp->cv, &tp->mu); } if (tp->head == 0 && tp->stop) { pthread_mutex_unlock(&tp->mu); break; } ct_pitem* it = tp->head; tp->head = it->nx; if (!tp->head) { tp->tail = 0; } pthread_mutex_unlock(&tp->mu); ct_task* t = (ct_task*)calloc(1, sizeof(ct_task)); pthread_mutex_init(&t->done_mu, 0); pthread_cond_init(&t->done_cv, 0); t->shim = it->shim; t->env = it->env; ct_shim_tramp((void*)t); ct_task_seg_free(t); free(t); free(it); } return 0; }
static ct_tpool* ct_pool_new(int n) { if (n < 1) { n = 1; } if (n > 16) { n = 16; } ct_tpool* tp = (ct_tpool*)calloc(1, sizeof(ct_tpool)); pthread_mutex_init(&tp->mu, 0); pthread_cond_init(&tp->cv, 0); tp->n = n; int i; for (i = 0; i < n; i++) { pthread_attr_t _pa; pthread_attr_init(&_pa); pthread_attr_setstacksize(&_pa, 8u << 20); pthread_create(&tp->th[i], &_pa, ct_pool_worker, tp); } return tp; }
static void ct_pool_spawn(ct_tpool* tp, void* (*shim)(void*), void* env) { ct_pitem* it = (ct_pitem*)malloc(sizeof(ct_pitem)); it->shim = shim; it->env = env; it->nx = 0; pthread_mutex_lock(&tp->mu); if (tp->tail) { tp->tail->nx = it; } else { tp->head = it; } tp->tail = it; pthread_cond_broadcast(&tp->cv); pthread_mutex_unlock(&tp->mu); }
static void* ct_shim_tramp(void* p);
static void ct_task_seg_free(ct_task* t);
static void ct_scope_join_all(ct_scope* sc) { if (!sc) { return; } ct_task* t = sc->head; while (t) { ct_task* nx = t->snext; if (!t->joined) { ct_join(t); } t = nx; } }
static ct_res ct_join_t(ct_task* t, long long ms) { ct_res res; if (ct_rt_active()) { ctron_rt_join_key((void*)t); t->joined = 1; ct_task_seg_free(t); if (t->state == 2) { res.variant = 1; res.v = (ct_i)(long)"task panic"; } else { res.variant = 0; res.v = t->result; } return res; } struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += ms / 1000; ts.tv_nsec += (ms % 1000) * 1000000L; if (ts.tv_nsec >= 1000000000L) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000L; } pthread_mutex_lock(&t->done_mu); while (!t->done) { ct_task* self2 = ct_task_self(); ct_scope* sc2 = self2 ? self2->scope : 0; if (sc2 && sc2->cancelled) { pthread_mutex_unlock(&t->done_mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } int wr = pthread_cond_timedwait(&t->done_cv, &t->done_mu, &ts); if (wr != 0) { break; } } pthread_mutex_unlock(&t->done_mu); if (!t->done) { res.variant = 1; res.v = (ct_i)(long)"JoinTimeout"; return res; } t->joined = 1; ct_task_seg_free(t); if (t->state == 2) { res.variant = 1; res.v = (ct_i)(long)"task panic"; } else { res.variant = 0; res.v = t->result; } return res; }
static void ct_scope_cancel(ct_scope* sc) { if (!sc) { return; } sc->cancelled = 1; ct_cancel_broadcast(); }
static const char* ct_xsend_str(const char* s) { size_t n = s ? strlen(s) + 1 : 1; char* r = (char*)malloc(n); if (!r) { ctron_panic("chan send OOM"); } if (s) { memcpy(r, s, n); } else { r[0] = 0; } return r; }
static const char* ct_xrecv_str(const char* s) { size_t n = strlen(s) + 1; char* r; if (ct_tls_arn || !ct_gc_enabled()) { r = (char*)ctron_amalloc(n); } else { r = (char*)ct_gc_alloc(n); } memcpy(r, s, n); free((void*)s); return r; }
static ct_chan* ct_ch_make(int cap, int is_str) { if (cap < 1) { cap = 1; } ct_chan* c = (ct_chan*)calloc(1, sizeof(ct_chan)); pthread_mutex_init(&c->mu, 0); pthread_cond_init(&c->cv_full, 0); pthread_cond_init(&c->cv_empty, 0); c->cap = cap; c->is_str = is_str; c->buf = (ct_i*)calloc((size_t)cap, sizeof(ct_i)); ct_ch_reg(c); return c; }
static ct_res ct_ch_send(ct_chan* c, ct_i v) { ct_res res; if (c->is_str) { v = (ct_i)(long)ct_xsend_str((const char*)(long)v); } pthread_mutex_lock(&c->mu); while (c->cnt == c->cap) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); if (c->is_str) { free((void*)(long)v); } res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_full, &c->mu); } c->buf[c->tail] = v; c->tail = (c->tail + 1) % c->cap; c->cnt += 1; pthread_cond_broadcast(&c->cv_empty); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); res.variant = 0; res.v = 0; return res; }
static ct_res ct_ch_recv(ct_chan* c) { ct_res res; pthread_mutex_lock(&c->mu); while (c->cnt == 0) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } pthread_cond_wait(&c->cv_empty, &c->mu); } res.variant = 0; res.v = c->buf[c->head]; if (c->is_str) { res.v = (ct_i)(long)ct_xrecv_str((const char*)(long)res.v); } c->head = (c->head + 1) % c->cap; c->cnt -= 1; pthread_cond_broadcast(&c->cv_full); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); return res; }
static ct_res ct_ch_recv_t(ct_chan* c, long long ms) { ct_res res; struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += ms / 1000; ts.tv_nsec += (ms % 1000) * 1000000L; if (ts.tv_nsec >= 1000000000L) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000L; } pthread_mutex_lock(&c->mu); while (c->cnt == 0) { ct_task* _ct = ct_task_self(); ct_scope* sc = _ct ? _ct->scope : 0; if (sc && sc->cancelled) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"ScopeCancelled"; return res; } if (ct_rt_coro_ctx()) { pthread_mutex_unlock(&c->mu); ctron_rt_park(); pthread_mutex_lock(&c->mu); continue; } int wr = pthread_cond_timedwait(&c->cv_empty, &c->mu, &ts); if (wr != 0 && c->cnt == 0) { pthread_mutex_unlock(&c->mu); res.variant = 1; res.v = (ct_i)(long)"RecvTimeout"; return res; } } res.variant = 0; res.v = c->buf[c->head]; if (c->is_str) { res.v = (ct_i)(long)ct_xrecv_str((const char*)(long)res.v); } c->head = (c->head + 1) % c->cap; c->cnt -= 1; pthread_cond_broadcast(&c->cv_full); pthread_mutex_unlock(&c->mu); if (ct_rt_active()) ctron_rt_cancel_wake_all(); return res; }
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
static ctron_list* ct_pmap_par(ctron_list* a, ct_fn1 f) { ctron_list* r = ctron_list_new(); if (a->n <= 0) return r; r->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)a->n); r->cap = a->n; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pmap_arg ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > a->n) ag[k].hi = a->n; if (ag[k].lo >= ag[k].hi) { ag[k].hi = ag[k].lo; } pthread_create(&th[k], 0, ct_pmap_work, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = a->n; return r; }
typedef struct { ctron_list* src; ct_fn2 f; ct_i init; int lo, hi; ct_i out; } ct_pred_arg;
static void* ct_pred_work(void* p) { ct_pred_arg* a = (ct_pred_arg*)p; ct_i acc = a->init; for (int i = a->lo; i < a->hi; i++) acc = a->f(acc, (ct_i)(long)a->src->items[i]); a->out = acc; return 0; }
static ct_i ct_pred_par(ctron_list* a, ct_i init, ct_fn2 f) { if (a->n <= 0) return init; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pred_arg ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > a->n) ag[k].hi = a->n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; pthread_create(&th[k], 0, ct_pred_work, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; int first = 1; for (int k = 0; k < K; k++) { if (ag[k].lo >= ag[k].hi) continue; if (first) { acc = ag[k].out; first = 0; } else { acc = f(acc, ag[k].out); } } return acc; }
static ctron_list* ct_pmap_par_a(void* base, int n, int w64, ct_fn1 f) { ctron_list* r = ctron_list_new(); if (n <= 0) return r; r->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)n); r->cap = n; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pmap_arga ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; if (ag[k].lo >= ag[k].hi) ag[k].hi = ag[k].lo; pthread_create(&th[k], 0, ct_pmap_work_a, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = n; return r; }
static ct_i ct_pred_par_a(void* base, int n, int w64, ct_fn2 f, ct_i init) { if (n <= 0) return init; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pred_arga ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; if (ag[k].lo > ag[k].hi) ag[k].lo = ag[k].hi; pthread_create(&th[k], 0, ct_pred_work_a, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; int first = 1; for (int k = 0; k < K; k++) { if (ag[k].lo >= ag[k].hi) continue; if (first) { acc = ag[k].out; first = 0; } else { acc = f(acc, ag[k].out); } } return acc; }
static ct_i ct_parallel_reduceL(ctron_list* a, ct_i init, ct_fn2 f) { ct_i acc = init; int i; for (i = 0; i < a->n; i++) { acc = f(acc, (ct_i)(long)a->items[i]); } return acc; }
static ct_i ct_clop_call1(ct_clop f, ct_i x) { if (f->env == (void*)1) { return ((ct_i(*)(ct_i))(f->fn))(x); } return ((ct_i(*)(void*, ct_i))(f->fn))(f->env, x); }
static ct_i ct_clop_call2(ct_clop f, ct_i a, ct_i b) { if (f->env == (void*)1) { return ((ct_i(*)(ct_i, ct_i))(f->fn))(a, b); } return ((ct_i(*)(void*, ct_i, ct_i))(f->fn))(f->env, a, b); }
typedef struct { ctron_list* src; ctron_list* dst; ct_clop f; int lo, hi; } ct_pmap_argc;
static void* ct_pmap_work_c(void* p) { ct_pmap_argc* a = (ct_pmap_argc*)p; for (int i = a->lo; i < a->hi; i++) a->dst->items[i] = (char*)(long)ct_clop_call1(a->f, (ct_i)(long)a->src->items[i]); return 0; }
static ctron_list* ct_pmap_par_c(ctron_list* a, ct_clop f) { ctron_list* r = ctron_list_new(); if (a->n <= 0) return r; r->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)a->n); r->cap = a->n; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pmap_argc ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > a->n) ag[k].hi = a->n; pthread_create(&th[k], 0, ct_pmap_work_c, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = a->n; return r; }
typedef struct { ctron_list* src; ct_clop f; ct_i init; ct_i out; int lo, hi; } ct_pred_argc;
static void* ct_pred_work_c(void* p) { ct_pred_argc* a = (ct_pred_argc*)p; ct_i acc = a->init; for (int i = a->lo; i < a->hi; i++) acc = ct_clop_call2(a->f, acc, (ct_i)(long)a->src->items[i]); a->out = acc; return 0; }
static ct_i ct_pred_par_c(ctron_list* a, ct_i init, ct_clop f) { if (a->n <= 0) return init; int K = a->n < 8 ? a->n : 8; pthread_t th[8]; ct_pred_argc ag[8]; int per = (a->n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].src = a; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; pthread_create(&th[k], 0, ct_pred_work_c, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; for (int k = 0; k < K; k++) acc = ct_clop_call2(f, acc, ag[k].out); return acc; }
typedef struct { void* base; ctron_list* dst; ct_clop f; int w64, lo, hi; } ct_pmap_argac;
static void* ct_pmap_work_ac(void* p) { ct_pmap_argac* a = (ct_pmap_argac*)p; for (int i = a->lo; i < a->hi; i++) { ct_i v = a->w64 ? ((int64_t*)a->base)[i] : (ct_i)((int32_t*)a->base)[i]; a->dst->items[i] = (char*)(long)ct_clop_call1(a->f, v); } return 0; }
static ctron_list* ct_pmap_par_ac(void* base, int n, int w64, ct_clop f) { ctron_list* r = ctron_list_new(); if (n <= 0) return r; r->items = (char**)ct_gc_alloc(sizeof(char*) * (size_t)n); r->cap = n; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pmap_argac ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].dst = r; ag[k].f = f; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; pthread_create(&th[k], 0, ct_pmap_work_ac, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); r->n = n; return r; }
typedef struct { void* base; ct_clop f; ct_i init; ct_i out; int w64, lo, hi; } ct_pred_argac;
static void* ct_pred_work_ac(void* p) { ct_pred_argac* a = (ct_pred_argac*)p; ct_i acc = a->init; for (int i = a->lo; i < a->hi; i++) { ct_i v = a->w64 ? ((int64_t*)a->base)[i] : (ct_i)((int32_t*)a->base)[i]; acc = ct_clop_call2(a->f, acc, v); } a->out = acc; return 0; }
static ct_i ct_pred_par_ac(void* base, int n, int w64, ct_clop f, ct_i init) { if (n <= 0) return init; int K = n < 8 ? n : 8; pthread_t th[8]; ct_pred_argac ag[8]; int per = (n + K - 1) / K; for (int k = 0; k < K; k++) { ag[k].base = base; ag[k].w64 = w64; ag[k].f = f; ag[k].init = init; ag[k].lo = k * per; ag[k].hi = (k + 1) * per; if (ag[k].hi > n) ag[k].hi = n; pthread_create(&th[k], 0, ct_pred_work_ac, &ag[k]); } for (int k = 0; k < K; k++) pthread_join(th[k], 0); ct_i acc = init; int first = 1; for (int k = 0; k < K; k++) { if (ag[k].lo >= ag[k].hi) continue; if (first) { acc = ag[k].out; first = 0; } else { acc = ct_clop_call2(f, acc, ag[k].out); } } return acc; }
static const char* ctron_cli_input = NULL;
static const char* ctron_entry(void) { return ctron_cli_input ? ctron_cli_input : ""; }
static const char* ctron_anchor = "";
static const char* ctron_embed_src = "";
static const char* ctron_embedded(void) { return ctron_embed_src; }
static const char* ctron_gui_entry_val = "app.ctml";
static const char* ctron_gui_entry(void) { return ctron_gui_entry_val; }
static const char* ctron_dom_title_buf = "";
static const char* ctron_dom_body_buf = "";
#if defined(__wasm__)
__attribute__((import_module("env"), import_name("ct_dom_set_title"))) extern void ct_dom_set_title(const char* s, unsigned n);
__attribute__((import_module("env"), import_name("ct_dom_set_body"))) extern void ct_dom_set_body(const char* s, unsigned n);
#endif
static void ctron_dom_set_title(const char* s) { ctron_dom_title_buf = s;
#if defined(__wasm__)
ct_dom_set_title(s, (unsigned)strlen(s));
#endif
}
static void ctron_dom_set_body(const char* s) { ctron_dom_body_buf = s;
#if defined(__wasm__)
ct_dom_set_body(s, (unsigned)strlen(s));
#endif
}
static const char* ctron_dom_title(void) { return ctron_dom_title_buf; }
static const char* ctron_dom_body(void) { return ctron_dom_body_buf; }
static int32_t ctron_char_len(const char* s) { int32_t n = 0; while (*s) { if ((*s & 0xC0) != 0x80) { n += 1; } s += 1; } return n; }
static const char* ctron_read_file_cli(const char* p) { if (ctron_cli_input) { return ctron_read_file(ctron_cli_input); } return ctron_read_file(p); }
#if !defined(_WIN32)
static char ctron_ext_fretbuf[64];
const char* ctron_ext_fret(void) { return ctron_ext_fretbuf; }
static double ctron_ext_f32slot(float fv) { double dx = 0.0; unsigned ctron_f32u; memcpy(&ctron_f32u, &fv, 4);
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    memcpy(((char*)&dx) + 4, &ctron_f32u, 4);
#else
    memcpy(((char*)&dx) + 0, &ctron_f32u, 4);
#endif
    return dx; }
double ctron_fx_scale(double v, double k) { return v * k; }
float ctron_fx_half(float v) { return v / 2.0f; }
int ctron_fx_f2i(double v) { return (int)v; }
double ctron_fx_mix(double v, int64_t sh) { double r = v; while (sh > 0) { r = r * 2.0; sh--; } return r; }
static long ctron_ext_dispatch0(const char* nm, int n, const char** fr) {
    void* p; long u[12]; double d[12]; unsigned fm = 0; int i; int k = 0;
    char rc = 'i'; const char* sym = nm;
    if (nm[0] == 'R' && nm[1] && nm[2] == ':') { rc = nm[1]; sym = nm + 3; }
    for (i = 0; i < n; i++) {
        if (!fr[i] || !fr[i][0]) { break; }
        if (fr[i][1] != ':') { ctron_panic("extern 帧损坏"); }
        if (getenv("CTRON_GUI_TRACE") != 0) { fprintf(stderr, "FR[%d]=%s\n", i, fr[i] ? fr[i] : "(null)"); }
        if (fr[i][0] == 'i') { u[k++] = strtol(fr[i] + 2, 0, 10); } else { if (fr[i][0] == 's') { u[k++] = (long)(fr[i] + 2); } else { if (fr[i][0] == 'f') { d[k] = strtod(fr[i] + 2, 0); fm |= 1u << k; k++; } else { if (fr[i][0] == 'g') { d[k] = ctron_ext_f32slot(strtof(fr[i] + 2, 0)); fm |= 1u << k; k++; } else { ctron_panic("extern 解释口径:不支持参数类型"); } } } }
    }
    if (getenv("CTRON_GUI_TRACE") != 0) { fprintf(stderr, "DISPATCH nm=%s n=%d k=%d\n", nm, n, k); }
    p = dlsym(RTLD_DEFAULT, sym);
    if (!p) { ctron_panic("extern 符号未找到(解释口径:符号须链接进解释器宿主)"); }
    if (rc == 'i' && fm == 0) {
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
    switch ((((rc == 'f') ? 1u : (rc == 'g') ? 2u : 0u) << 28) | (((unsigned)k) << 24) | fm) {
        case 268435456: { double r = ((double (*)())p)(); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 285212672: { double r = ((double (*)(long))p)(u[0]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 285212673: { double r = ((double (*)(double))p)(d[0]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 301989888: { double r = ((double (*)(long, long))p)(u[0], u[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 301989889: { double r = ((double (*)(double, long))p)(d[0], u[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 301989890: { double r = ((double (*)(long, double))p)(u[0], d[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 301989891: { double r = ((double (*)(double, double))p)(d[0], d[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767104: { double r = ((double (*)(long, long, long))p)(u[0], u[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767105: { double r = ((double (*)(double, long, long))p)(d[0], u[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767106: { double r = ((double (*)(long, double, long))p)(u[0], d[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767107: { double r = ((double (*)(double, double, long))p)(d[0], d[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767108: { double r = ((double (*)(long, long, double))p)(u[0], u[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767109: { double r = ((double (*)(double, long, double))p)(d[0], u[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767110: { double r = ((double (*)(long, double, double))p)(u[0], d[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 318767111: { double r = ((double (*)(double, double, double))p)(d[0], d[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544320: { double r = ((double (*)(long, long, long, long))p)(u[0], u[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544321: { double r = ((double (*)(double, long, long, long))p)(d[0], u[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544322: { double r = ((double (*)(long, double, long, long))p)(u[0], d[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544323: { double r = ((double (*)(double, double, long, long))p)(d[0], d[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544324: { double r = ((double (*)(long, long, double, long))p)(u[0], u[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544325: { double r = ((double (*)(double, long, double, long))p)(d[0], u[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544326: { double r = ((double (*)(long, double, double, long))p)(u[0], d[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544327: { double r = ((double (*)(double, double, double, long))p)(d[0], d[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544328: { double r = ((double (*)(long, long, long, double))p)(u[0], u[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544329: { double r = ((double (*)(double, long, long, double))p)(d[0], u[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544330: { double r = ((double (*)(long, double, long, double))p)(u[0], d[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544331: { double r = ((double (*)(double, double, long, double))p)(d[0], d[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544332: { double r = ((double (*)(long, long, double, double))p)(u[0], u[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544333: { double r = ((double (*)(double, long, double, double))p)(d[0], u[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544334: { double r = ((double (*)(long, double, double, double))p)(u[0], d[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 335544335: { double r = ((double (*)(double, double, double, double))p)(d[0], d[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", r); return 0; }
        case 536870912: { float r = ((float (*)())p)(); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 553648128: { float r = ((float (*)(long))p)(u[0]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 553648129: { float r = ((float (*)(double))p)(d[0]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 570425344: { float r = ((float (*)(long, long))p)(u[0], u[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 570425345: { float r = ((float (*)(double, long))p)(d[0], u[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 570425346: { float r = ((float (*)(long, double))p)(u[0], d[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 570425347: { float r = ((float (*)(double, double))p)(d[0], d[1]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202560: { float r = ((float (*)(long, long, long))p)(u[0], u[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202561: { float r = ((float (*)(double, long, long))p)(d[0], u[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202562: { float r = ((float (*)(long, double, long))p)(u[0], d[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202563: { float r = ((float (*)(double, double, long))p)(d[0], d[1], u[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202564: { float r = ((float (*)(long, long, double))p)(u[0], u[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202565: { float r = ((float (*)(double, long, double))p)(d[0], u[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202566: { float r = ((float (*)(long, double, double))p)(u[0], d[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 587202567: { float r = ((float (*)(double, double, double))p)(d[0], d[1], d[2]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979776: { float r = ((float (*)(long, long, long, long))p)(u[0], u[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979777: { float r = ((float (*)(double, long, long, long))p)(d[0], u[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979778: { float r = ((float (*)(long, double, long, long))p)(u[0], d[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979779: { float r = ((float (*)(double, double, long, long))p)(d[0], d[1], u[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979780: { float r = ((float (*)(long, long, double, long))p)(u[0], u[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979781: { float r = ((float (*)(double, long, double, long))p)(d[0], u[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979782: { float r = ((float (*)(long, double, double, long))p)(u[0], d[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979783: { float r = ((float (*)(double, double, double, long))p)(d[0], d[1], d[2], u[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979784: { float r = ((float (*)(long, long, long, double))p)(u[0], u[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979785: { float r = ((float (*)(double, long, long, double))p)(d[0], u[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979786: { float r = ((float (*)(long, double, long, double))p)(u[0], d[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979787: { float r = ((float (*)(double, double, long, double))p)(d[0], d[1], u[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979788: { float r = ((float (*)(long, long, double, double))p)(u[0], u[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979789: { float r = ((float (*)(double, long, double, double))p)(d[0], u[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979790: { float r = ((float (*)(long, double, double, double))p)(u[0], d[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 603979791: { float r = ((float (*)(double, double, double, double))p)(d[0], d[1], d[2], d[3]); snprintf(ctron_ext_fretbuf, 64, "%.17g", (double)r); return 0; }
        case 16777217: return ((long (*)(double))p)(d[0]);
        case 33554433: return ((long (*)(double, long))p)(d[0], u[1]);
        case 33554434: return ((long (*)(long, double))p)(u[0], d[1]);
        case 33554435: return ((long (*)(double, double))p)(d[0], d[1]);
        case 50331649: return ((long (*)(double, long, long))p)(d[0], u[1], u[2]);
        case 50331650: return ((long (*)(long, double, long))p)(u[0], d[1], u[2]);
        case 50331651: return ((long (*)(double, double, long))p)(d[0], d[1], u[2]);
        case 50331652: return ((long (*)(long, long, double))p)(u[0], u[1], d[2]);
        case 50331653: return ((long (*)(double, long, double))p)(d[0], u[1], d[2]);
        case 50331654: return ((long (*)(long, double, double))p)(u[0], d[1], d[2]);
        case 50331655: return ((long (*)(double, double, double))p)(d[0], d[1], d[2]);
        case 67108865: return ((long (*)(double, long, long, long))p)(d[0], u[1], u[2], u[3]);
        case 67108866: return ((long (*)(long, double, long, long))p)(u[0], d[1], u[2], u[3]);
        case 67108867: return ((long (*)(double, double, long, long))p)(d[0], d[1], u[2], u[3]);
        case 67108868: return ((long (*)(long, long, double, long))p)(u[0], u[1], d[2], u[3]);
        case 67108869: return ((long (*)(double, long, double, long))p)(d[0], u[1], d[2], u[3]);
        case 67108870: return ((long (*)(long, double, double, long))p)(u[0], d[1], d[2], u[3]);
        case 67108871: return ((long (*)(double, double, double, long))p)(d[0], d[1], d[2], u[3]);
        case 67108872: return ((long (*)(long, long, long, double))p)(u[0], u[1], u[2], d[3]);
        case 67108873: return ((long (*)(double, long, long, double))p)(d[0], u[1], u[2], d[3]);
        case 67108874: return ((long (*)(long, double, long, double))p)(u[0], d[1], u[2], d[3]);
        case 67108875: return ((long (*)(double, double, long, double))p)(d[0], d[1], u[2], d[3]);
        case 67108876: return ((long (*)(long, long, double, double))p)(u[0], u[1], d[2], d[3]);
        case 67108877: return ((long (*)(double, long, double, double))p)(d[0], u[1], d[2], d[3]);
        case 67108878: return ((long (*)(long, double, double, double))p)(u[0], d[1], d[2], d[3]);
        case 67108879: return ((long (*)(double, double, double, double))p)(d[0], d[1], d[2], d[3]);
        default: ctron_panic("extern 解释口径:float 形状不支持(>4 参)"); return 0;
    }
}
int64_t ctron_ext_dispatch(const char* nm, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6, const char* a7, const char* a8, const char* a9, const char* a10, const char* a11) {
    const char* fr[12];
    fr[0] = a0; fr[1] = a1; fr[2] = a2; fr[3] = a3; fr[4] = a4; fr[5] = a5;
    fr[6] = a6; fr[7] = a7; fr[8] = a8; fr[9] = a9; fr[10] = a10; fr[11] = a11;
    return ctron_ext_dispatch0(nm, 12, fr);
}
#else
int64_t ctron_ext_dispatch(const char* nm, const char* a0, const char* a1, const char* a2, const char* a3, const char* a4, const char* a5, const char* a6, const char* a7, const char* a8, const char* a9, const char* a10, const char* a11) { (void)nm; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; ctron_panic("extern 解释口径:Windows β 不支持"); return 0; }
#endif
static const char* ctron_version = "v0.0.2-193-g8a2c3fd4";
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
int ctron_env_set(const char* n, const char* v) { return setenv(n, v, 1) == 0; }
static int ctron_argc_g = 0;
static char** ctron_argv_g = 0;
static int ctron_argv_off = 1;
static int64_t ctron_prog_args_n(void) { return (int64_t)(ctron_argc_g - ctron_argv_off); }
static const char* ctron_prog_args_at(int64_t i) { return (i >= 0 && i < (int64_t)(ctron_argc_g - ctron_argv_off)) ? ctron_argv_g[ctron_argv_off + i] : ""; }
typedef struct { int32_t* d; int64_t n; } ctron_view_i;
typedef struct { int64_t* d; int64_t n; } ctron_view_6;
typedef struct { void* base; int64_t off; int64_t cap; } ct_arena;
typedef struct { unsigned char* d; int64_t n; } ctron_bytes;
static ctron_bytes* ctron_read_file_bytes(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return NULL; } fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); ctron_bytes* b = (ctron_bytes*)ctron_amalloc(sizeof(ctron_bytes)); b->d = (unsigned char*)ctron_amalloc((size_t)(n > 0 ? n : 1)); size_t rd = n > 0 ? fread(b->d, 1, (size_t)n, f) : 0; b->n = (int64_t)rd; fclose(f); return b; }
static int64_t ctron_bytes_len(ctron_bytes* b) { return b ? b->n : 0; }
static int32_t ctron_bytes_at(ctron_bytes* b, int64_t i) { if (!b || i < 0 || i >= b->n) { ctron_panic("bytes index out of bounds"); } return (int32_t)b->d[i]; }
static ctron_bytes* ctron_bytes_slice(ctron_bytes* b, int64_t a, int64_t n) { if (!b || a < 0 || n < 0 || a + n > b->n) { ctron_panic("bytes slice range"); } ctron_bytes* r = (ctron_bytes*)ctron_amalloc(sizeof(ctron_bytes)); r->d = (unsigned char*)ctron_amalloc((size_t)(n > 0 ? n : 1)); memcpy(r->d, b->d + a, (size_t)n); r->n = n; return r; }
static const char* ctron_u8s_str(const uint8_t* p, int from, int len) { if (len < 0 || from < 0) { ctron_panic("u8s_str: range"); } char* r = (char*)ct_gc_alloc((size_t)len + 1); if (p && len > 0) { memcpy(r, p + from, (size_t)len); } r[len] = 0; return r; }
static const char* ctron_bytes_to_str(ctron_bytes* b) { int64_t n = 0; if (b) { n = b->n; for (int64_t i = 0; i < n; i++) { if (b->d[i] == 0) { n = i; break; } } } char* r = (char*)ctron_amalloc((size_t)(n + 1)); memcpy(r, b ? (const void*)b->d : (const void*)r, (size_t)n); r[n] = 0; return r; }
static long ctron_rbn_g = 0;
static unsigned char* ctron_rb_g = NULL;
static int64_t ctron_read_bytes_len(const char* path) { FILE* f = fopen(path, "rb"); if (!f) { return -1; } fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); if (ctron_rb_g) free(ctron_rb_g); ctron_rb_g = (unsigned char*)ctron_amalloc((size_t)(sz > 0 ? sz : 1)); size_t rd = sz > 0 ? fread(ctron_rb_g, 1, (size_t)sz, f) : 0; fclose(f); ctron_rbn_g = (long)rd; return (int64_t)rd; }
static int32_t ctron_read_bytes_at(int64_t i) { if (!ctron_rb_g || i < 0 || i >= (int64_t)ctron_rbn_g) { return 0; } return (int32_t)ctron_rb_g[i]; }
static void* ct_arena_bump(ct_arena* a, long long n) { if (n < 0 || (unsigned long long)n > (unsigned long long)(a->cap - a->off)) { ctron_panic("arena exhausted"); } void* p = (char*)a->base + a->off; a->off += n; return p; }
static void ct_arena_reset(ct_arena* a, long long m) { if (m < 0 || m > a->off) { ctron_panic("arena exhausted"); } a->off = m; }
static ct_arn* ct_region_cur(void) { return ct_tls_arn ? ct_tls_arn : &ctron_aglobal; }
static ct_arena ct_region_heap(void) { ct_arn* a = ct_region_cur(); ct_arena v; v.base = a->cur ? (void*)(a->cur + 1) : (void*)0; v.off = (long long)a->off; v.cap = a->cur ? (long long)a->cur->cap : 0; return v; }
static long long ct_region_mark(ct_arena* v) { ct_arn* a = ct_region_cur(); if (a->cur && (char*)(a->cur + 1) == (char*)v->base) { return (long long)a->off; } return v->off; }
static void ct_region_reset(ct_arena* v, long long m) { ct_arn* a = ct_region_cur(); if (a->cur && (char*)(a->cur + 1) == (char*)v->base) { if (m < 0 || m > (long long)a->off) { ctron_panic("region reset across growth"); } a->off = (size_t)m; return; } ct_arena_reset(v, m); }
typedef struct { void* base; int64_t esz; int64_t cap; int64_t free_head; } ct_pool;
static void ct_pool_init(ct_pool* p, void* base, long long esz, long long n) { if (esz < 8) { esz = 8; } p->base = base; p->esz = esz; p->cap = n; p->free_head = 0; { long long i = 0; while (i < n) { *(long long*)((char*)base + i * esz) = i + 1; i += 1; } } }
static long long ct_pool_alloc(ct_pool* p) { if (p->free_head >= p->cap) { ctron_panic("pool exhausted"); } long long i = p->free_head; p->free_head = *(long long*)((char*)p->base + i * p->esz); return i; }
static void ct_pool_free(ct_pool* p, long long i) { *(long long*)((char*)p->base + i * p->esz) = p->free_head; p->free_head = i; }
static ctron_view_i ct_arena_zeros_i(ct_arena* a, long long n) { ctron_view_i v; v.d = (int32_t*)ct_arena_bump(a, n * 4); memset(v.d, 0, (size_t)(n * 4)); v.n = n; return v; }
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
static int32_t t_test_0(void) {
struct ct_fr ct_frn __attribute__((cleanup(ct_fp_pop))); ct_frn.up = ct_fp; ct_frn.pn = 0; ct_frn.ovf = 0; ct_frn.spn = 0; ct_fp = &ct_frn;
if (!((1) == (1))) { ctron_panic("assert_eq failed"); }
return 0;
}
int main(int argc, char** argv) { ctron_argc_g = argc; ctron_argv_g = argv; ct_gc_stack_base = (char*)&argc; if (argc >= 2 && !strcmp(argv[1], "--version")) { puts(ctron_version); return 0; } { int ctron_ai; for (ctron_ai = 3; ctron_ai < argc; ctron_ai++) { if (!strncmp(argv[ctron_ai], "--format=", 9)) { ctron_cli_fmt = argv[ctron_ai] + 9; } else if (!strncmp(argv[ctron_ai], "--profile=", 10)) { ctron_cli_prof = argv[ctron_ai] + 10; } else if (!strcmp(argv[ctron_ai], "--trusted")) { ctron_cli_trusted = "1"; } else if (!strcmp(argv[ctron_ai], "--dump-gui")) { ctron_cli_dumpgui = "1"; } } } 
#if defined(_WIN32) 
SetConsoleOutputCP(CP_UTF8); 
_setmode(_fileno(stdout), _O_BINARY); 
#endif
 if (argc >= 3 && strcmp(argv[1], "run") == 0) { ctron_cli_input = argv[2]; ctron_argv_off = 3; } if (t_test_0()) { return 1; } return 0; }
