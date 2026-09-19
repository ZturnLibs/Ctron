// sb_shim.c —— SheenBidi 的 Ctron FFI 适配(M3 文本管线地基;s14 是 C 层冒烟,本夹具证明 Ctron 侧可达)
// 句柄口径:不透明引用走 I64 直通(shim 内 intptr_t 互转);平面签名,无结构体跨界
#include <SheenBidi/SheenBidi.h>
#include <stdint.h>
#include <string.h>

long long sb_open(const char* text) {
    if (!text) { return 0; }
    SBAlgorithmRef a = SBAlgorithmCreate(&(SBCodepointSequence){
        SBStringEncodingUTF8, (void*)text, (SBUInteger)strlen(text) });
    return (long long)(intptr_t)a;
}

long long sb_paragraph(long long alg, int len) {
    SBParagraphRef p = SBAlgorithmCreateParagraph(
        (SBAlgorithmRef)(intptr_t)alg, 0, (SBUInteger)len, SBLevelDefaultLTR);
    return (long long)(intptr_t)p;
}

long long sb_line(long long para) {
    SBParagraphRef p = (SBParagraphRef)(intptr_t)para;
    return (long long)(intptr_t)SBParagraphCreateLine(p, 0, SBParagraphGetLength(p));
}

int sb_run_count(long long line) {
    return (int)SBLineGetRunCount((SBLineRef)(intptr_t)line);
}

int sb_run_offset(long long line, int i) {
    return (int)SBLineGetRunsPtr((SBLineRef)(intptr_t)line)[i].offset;
}

int sb_run_length(long long line, int i) {
    return (int)SBLineGetRunsPtr((SBLineRef)(intptr_t)line)[i].length;
}

int sb_run_level(long long line, int i) {
    return (int)SBLineGetRunsPtr((SBLineRef)(intptr_t)line)[i].level;
}

void sb_close(long long line, long long para, long long alg) {
    if (line) { SBLineRelease((SBLineRef)(intptr_t)line); }
    if (para) { SBParagraphRelease((SBParagraphRef)(intptr_t)para); }
    if (alg) { SBAlgorithmRelease((SBAlgorithmRef)(intptr_t)alg); }
}
