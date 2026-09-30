/* tests/ffi/ptr_view —— qsort 比较器:C 侧回调 Ctron fn,指针经 ptr_as_view 读 */
#include <stdlib.h>
#include <stdint.h>

void my_qsort(int32_t *base, size_t n, size_t w,
              int (*cmp)(const int32_t *, const int32_t *)) {
    qsort(base, n, w, (int (*)(const void *, const void *))cmp);
}
