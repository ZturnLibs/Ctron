/* tests/ffi/layout —— C 侧:同属性镜像(Packed = __attribute__((packed)),
 * Wide = __attribute__((aligned(32))));与发射面互为 ABI 镜像。 */
#include <stdint.h>

typedef struct __attribute__((packed)) { int64_t a; int8_t b; int64_t c; } Packed;
typedef struct __attribute__((aligned(32))) { int64_t v; } Wide;

int64_t pk_sizeof(void) {
    return (int64_t)sizeof(Packed);
}

int64_t pk_read(Packed p) {
    return p.a + p.b + p.c;
}

int64_t wd_sizeof(void) {
    return (int64_t)sizeof(Wide);
}

int64_t wd_align(void) {
    return (int64_t)_Alignof(Wide);
}
