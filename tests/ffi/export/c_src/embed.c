/* tests/ffi/export —— C 宿主直链 Ctron 导出符号(嵌入面)。
 * 发射产物经 -Dmain=ctron_embed_main 重命名其自带 main 后与本文件同链;
 * 导出包装自带 statics 初始化守卫,based() 首调即完成 ctron_statics_init。 */
#include <stdio.h>
#include <stdint.h>

int64_t magic(int64_t x);
int64_t tally(int64_t a, int64_t b, int64_t c);
int64_t based(int64_t x);
int ctron_embed_main(int argc, char** argv);

int main(void) {
    if (magic(4) != 13) { puts("FAIL magic"); return 1; }
    if (tally(1, 2, 3) != 321) { puts("FAIL tally"); return 1; }
    if (based(1) != 1001) { puts("FAIL based(statics init)"); return 1; }
    if (based(5) != 1005) { puts("FAIL based(re)"); return 1; }
    puts("export embed OK");
    return 0;
}
