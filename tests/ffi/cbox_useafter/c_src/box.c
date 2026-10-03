/* cbox_useafter C 侧:最小计数分配器 */
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
long long box_audit(void) { return box_live; }
