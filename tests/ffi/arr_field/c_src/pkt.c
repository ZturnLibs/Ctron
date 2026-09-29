/* tests/ffi/arr_field —— C 实现:定长数组字段布局互证 */
#include <stdint.h>

typedef struct {
    int64_t id;
    int64_t buf[4];
    int32_t tag;
} Pkt;

Pkt pkt_make(int64_t id, int32_t tag) {
    Pkt p;
    p.id = id;
    p.buf[0] = 1;
    p.buf[1] = 2;
    p.buf[2] = 3;
    p.buf[3] = 4;
    p.tag = tag;
    return p;
}

int64_t pkt_bufsum(Pkt p) {
    return p.buf[0] + p.buf[1] + p.buf[2] + p.buf[3];
}

int64_t pkt_sizeof(void) {
    return (int64_t)sizeof(Pkt);
}
