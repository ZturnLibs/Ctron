/* tests/ffi/clo_cb —— C 侧:回调槽 + 触发(ctx 由 C 原样回传)。 */
#include <stdint.h>

typedef int64_t (*slot_fn)(int64_t, void*);

static slot_fn slot = 0;
static void* slot_user = 0;

void set_cb(int64_t (*f)(int64_t, void*), void* user) {
    slot = f;
    slot_user = user;
}

int64_t fire(int64_t ev) {
    return slot(ev, slot_user);
}
