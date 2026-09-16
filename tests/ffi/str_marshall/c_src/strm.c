/* tests/ffi/str_marshall —— C 侧:借用读 / 返回 C-owned 串(malloc 与静态存储)。 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* size_t 垫片:Darwin 上 size_t(unsigned long)与发射面 uint64_t(unsigned long long)
 * 是不同类型别名,直接声明 libc strlen 会重定义冲突(见 main.ct 注)。 */
int64_t c_strlen(const char* s) {
    return (int64_t)strlen(s);
}

int64_t c_count_vowels(const char* s) {
    int64_t n = 0;
    for (; *s; s++) {
        if (strchr("aeiouAEIOU", *s)) {
            n += 1;
        }
    }
    return n;
}

/* C-owned(Ctron-owned 副本由 str_from_c 深拷获得;demo 从简,免 free 挂账 C 侧) */
const char* c_shout(const char* s) {
    size_t n = strlen(s);
    char* r = (char*)malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        r[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
    }
    r[n] = 0;
    return r;
}

const char* c_static_greet(void) {
    return "hello from C";
}
