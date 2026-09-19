// smoke.c —— SheenBidi vendored 冒烟:混合双向文本 run/level 断言(M3 bidi 管线地基)
// "Hi " + "سلام"(UTF-8 8 字节):LTR 基向下应切两段 run——拉丁段 level 0,阿拉伯段 level 1
#include <SheenBidi/SheenBidi.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* text = "Hi \xD8\xB3\xD9\x84\xD8\xA7\xD9\x85";
    SBUInteger len = (SBUInteger)strlen(text);

    SBAlgorithmRef alg = SBAlgorithmCreate(&(SBCodepointSequence){
        SBStringEncodingUTF8, (void*)text, len });
    if (!alg) { printf("bidi: algorithm FAIL\n"); return 1; }

    SBParagraphRef para = SBAlgorithmCreateParagraph(alg, 0, len, SBLevelDefaultLTR);
    if (!para) { printf("bidi: paragraph FAIL\n"); return 1; }

    SBLineRef line = SBParagraphCreateLine(para, 0, SBParagraphGetLength(para));
    if (!line) { printf("bidi: line FAIL\n"); return 1; }

    SBUInteger n = SBLineGetRunCount(line);
    const SBRun* runs = SBLineGetRunsPtr(line);
    printf("bidi: run count = %u\n", (unsigned)n);
    for (SBUInteger i = 0; i < n; i++) {
        printf("  run %u: offset=%u len=%u level=%d\n", (unsigned)i,
               (unsigned)runs[i].offset, (unsigned)runs[i].length, (int)runs[i].level);
    }

    int ok = (n == 2)
        && runs[0].offset == 0 && runs[0].length == 3 && runs[0].level == 0
        && runs[1].offset == 3 && runs[1].length == 8 && runs[1].level == 1;

    SBLineRelease(line);
    SBParagraphRelease(para);
    SBAlgorithmRelease(alg);

    if (ok) {
        printf("bidi: LTR/RTL run 切分 PASS\n");
        return 0;
    }
    printf("bidi: run 断言 FAIL\n");
    return 1;
}
