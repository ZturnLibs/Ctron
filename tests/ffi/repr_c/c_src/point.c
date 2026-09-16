/* tests/ffi/repr_c —— C 侧:同型 struct 定义(声明序镜像 #[repr(c)] Point)。 */
#include <stdint.h>

typedef struct { int64_t x; double y; int32_t tag; } Point;

Point pt_make(int64_t x, double y, int32_t tag) {
    Point p;
    p.x = x;
    p.y = y;
    p.tag = tag;
    return p;
}

double pt_sum(Point p) {
    return (double)p.x + p.y;
}

Point pt_bump(Point p) {
    p.tag += 1;
    return p;
}

int64_t pt_sizeof(void) {
    return (int64_t)sizeof(Point);
}
