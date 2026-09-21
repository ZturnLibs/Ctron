// arena.c —— 块链 bump 分配器。分配永不移动;块链头插,整体遍历释放。
#include "arena.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ARENA_MIN_BLOCK = 1 << 16, ARENA_ALIGN = 16 };

// CTRON_MEM_DEBUG=1:需求量记账(总需求/块数/进度标记;env 门控,常态零成本)。
// 用途:linux seed 内存爆炸定界(docs/linux-seed-memory-evidence.md)——
// 若 linux 总需求 ≈ macOS 而 RSS 7× → 分配器驻留;若需求本身 7× → 执行分歧。
static long g_mem_total = 0;   // 累计需求字节
static long g_mem_next = 0;    // 下一个进度标记阈值(256MB 步进)
static int g_mem_on = -1;
static int g_mem_blocks = 0;
static long g_extend_liar = 0;
static long g_hist[4];
static long g_hist_n;
// 按"当前 Ctron 函数"归因(CTRON_FN_TRACE=1):512 桶开地址散列,键=函数名指针
const char* g_cur_fn = NULL;
static int g_cur_fn_set = -1;
typedef struct { const char* name; long bytes; long calls; } fnbkt;
static fnbkt g_fnb[512];
static long g_attr_bytes = 0;
static long g_attr_miss = 0;
static int g_dumped = 0;
static int g_fn_dumped = 0;
void ctron_fn_enter(const char* n) {
    unsigned h = (unsigned)((uintptr_t)n >> 4) & 511u;
    while (g_fnb[h].name && g_fnb[h].name != n) h = (h + 1) & 511u;
    g_fnb[h].name = n;
    g_fnb[h].calls++;
    g_cur_fn = n;
}
typedef struct blk {
    struct blk* next;
    size_t cap;   // 本块可分配字节数
    size_t used;  // 已用字节数
} blk;

struct ctron_arena { blk* head; void* gnext; };
static ctron_arena* g_arenas = NULL;

static void mem_report(void);
static void mem_final(void) {
    g_dumped = 1;
    mem_report();
}
static void mem_report(void) {
    fprintf(stderr, "MEM total=%ldMB blocks=%d liar=%ld allocs=%ld attr=%ldMB miss=%ldMB\n",
           g_mem_total >> 20, g_mem_blocks, g_extend_liar, g_hist_n,
           g_attr_bytes >> 20, g_attr_miss >> 20);
    if (g_cur_fn_set == 1 && g_dumped && !g_fn_dumped) {
        g_fn_dumped = 1;
        for (int t = 0; t < 20; t++) {
            int best = -1;
            for (int q = 0; q < 512; q++)
                if (g_fnb[q].name && g_fnb[q].bytes >= 0 &&
                    (best < 0 || g_fnb[q].bytes > g_fnb[best].bytes)) best = q;
            if (best < 0 || g_fnb[best].bytes <= 0) break;
            fprintf(stderr, "FN %-28s %8ldMB %10ld calls\n", g_fnb[best].name,
                    g_fnb[best].bytes >> 20, g_fnb[best].calls);
            g_fnb[best].bytes = -1;
        }
    }
}

// 退出时逐块对账:cap 合计 vs 实际 touched(used)——比值 = 虚拟浪费倍率
void ctron_mem_audit(const ctron_arena* a) {
    if (!g_mem_on || !a) return;
    long cap_sum = 0, used_sum = 0;
    int n = 0;
    for (const blk* b = a->head; b; b = b->next) {
        cap_sum += (long)(b->cap + 64);
        used_sum += (long)b->used;
        n++;
        if (b->cap >= (1u << 20))
            fprintf(stderr, "AUDIT blk#%d cap=%zuMB used=%zuMB\n", n, b->cap >> 20, b->used >> 20);
    }
    fprintf(stderr, "AUDIT SUM cap=%ldMB used=%ldMB blocks=%d\n", cap_sum >> 20, used_sum >> 20, n);
}

static void mem_tick(long n) {
    if (g_mem_on < 0) g_mem_on = getenv("CTRON_MEM_DEBUG") ? 1 : 0;
    if (!g_mem_on) return;
    g_mem_total += n;
    if (g_mem_total >= g_mem_next) {
        g_mem_next += (256L << 20);
        mem_report();
    }
}

long ctron_mem_total(void) { return g_mem_total; }

static size_t align_up(size_t n) { return (n + ARENA_ALIGN - 1) & ~(size_t)(ARENA_ALIGN - 1); }
static size_t blk_payload_off(void) { return align_up(sizeof(blk)); }

static blk* blk_new(size_t cap) {
    blk* b = (blk*)malloc(blk_payload_off() + cap);
    if (!b) abort();
    b->next = NULL;
    b->cap = cap;
    b->used = 0;
    return b;
}

ctron_arena* ctron_arena_new(void) {
    ctron_arena* a = (ctron_arena*)calloc(1, sizeof(ctron_arena));
    if (!a) abort();
    if (g_mem_on < 0) {
        g_mem_on = getenv("CTRON_MEM_DEBUG") ? 1 : 0;
        if (getenv("CTRON_FN_TRACE")) g_cur_fn_set = 1;
        if (g_mem_on) atexit(mem_final);
    }
    if (g_mem_on) {
        a->gnext = g_arenas;
        g_arenas = a;
    }
    return a;
}

void* ctron_arena_alloc(ctron_arena* a, size_t n) {
    n = align_up(n ? n : 1);
    if (!a->head) a->head = blk_new(n < ARENA_MIN_BLOCK ? ARENA_MIN_BLOCK : n);
    if (a->head->used + n > a->head->cap) {
        size_t c = a->head->cap * 2;
        if (n > c) c = n;
        blk* b = blk_new(c);
        b->next = a->head;
        a->head = b;
        g_mem_blocks++;
        if (g_mem_on && c >= (1u << 20))
            fprintf(stderr, "BLK cap=%zuMB prev_used=%zuMB\n", c >> 20,
                    a->head == b ? 0 : (a->head->used >> 20));
        mem_tick((long)c);
    }
    void* p = (char*)a->head + blk_payload_off() + a->head->used;
    a->head->used += n;
    memset(p, 0, n);
    if (g_mem_on) {
        static long b0, b1c, b2, b3; // <1KB / 1K-64K / 64K-1M / >1M 的字节量
        if (n < 1024) b0 += n;
        else if (n < 65536) b1c += n;
        else if (n < 1048576) b2 += n;
        else b3 += n;
        g_hist[0] = b0; g_hist[1] = b1c; g_hist[2] = b2; g_hist[3] = b3;
        g_hist_n++;
        if (g_cur_fn_set == 1 && g_cur_fn) {
            unsigned h = (unsigned)((uintptr_t)g_cur_fn >> 4) & 511u;
            while (g_fnb[h].name && g_fnb[h].name != g_cur_fn) h = (h + 1) & 511u;
            g_fnb[h].name = g_cur_fn;
            g_fnb[h].bytes += (long)n;
            g_attr_bytes += (long)n;
        } else {
            g_attr_miss += (long)n;
        }
    }
    return p;
}

int ctron_arena_try_extend(ctron_arena* a, void* p, size_t old_len, size_t new_len) {
    if (!a->head || !p) return 0;
    size_t old_foot = align_up(old_len ? old_len : 1);
    size_t new_foot = align_up(new_len ? new_len : 1);
    if (new_foot <= old_foot) return new_foot == old_foot ? 1 : 0;
    char* pend = (char*)p + old_foot;
    char* hend = (char*)a->head + blk_payload_off() + a->head->used;
    // 仅当 p 是 head 块的最后一次分配且尾部余量足够时原地扩展(零拷贝拼接快路径)
    if (pend != hend) return 0;
    if ((char*)p < (char*)a->head + blk_payload_off()) return 0;
    if (a->head->used + (new_foot - old_foot) > a->head->cap) return 0;
    // 不变量自检:若 p 真是末次分配,其 NUL 之后至 footprint 末尾应保持分配时的清零态;
    // 违例 = 判定不充分(有别名写),退回拷贝路径并计数
    for (size_t q = old_len; q < old_foot; q++) {
        if (((char*)p)[q] != 0) {
            g_extend_liar++;
            return 0;
        }
    }
    a->head->used += new_foot - old_foot;
    return 1;
}

char* ctron_arena_strndup(ctron_arena* a, const void* p, size_t n) {
    char* s = (char*)ctron_arena_alloc(a, n + 1);
    memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

void ctron_arena_free(ctron_arena* a) {
    if (!a) return;
    if (g_mem_on) ctron_mem_audit(a);
    blk* b = a->head;
    while (b) {
        blk* nx = b->next;
        free(b);
        b = nx;
    }
    free(a);
}
