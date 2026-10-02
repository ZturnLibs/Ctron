/* cbox 夹具 C 侧:计数分配器(审计 malloc/destroy 平衡;box free 面经
   lib/ffi ctron_cbox_free 走 libc free,由守卫负锚与单次执行语义钉住) */
#include <stdlib.h>
typedef struct Thing { long long v; } Thing;

static long long box_live = 0;

Thing* thing_make(long long v) {
    Thing* t = (Thing*)malloc(sizeof(Thing));
    if (!t) return 0;
    t->v = v;
    box_live += 1;
    return t;
}
void thing_bump(Thing* t) { t->v += 1; }
long long thing_value(Thing* t) { return t->v; }
void thing_destroy(Thing* t) { if (t) { box_live -= 1; free(t); } }
long long box_audit(void) { return box_live; }

void* box_raw(long long n) { return malloc((size_t)n); }
void box_raw_poke(void* p, long long i, unsigned char b) { ((unsigned char*)p)[i] = b; }
long long box_raw_peek(void* p, long long i) { return (long long)((unsigned char*)p)[i]; }
