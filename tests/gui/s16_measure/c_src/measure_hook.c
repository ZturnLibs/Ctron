// measure_hook.c —— FreeType 实测宽度接入 Clay(s16;gui 的 0.55 系数桩的升级证明)
// 口径:fixture 本地重设 Clay_SetMeasureTextFunction,不动共享 gui;上游合并留后续
#include <clay.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string.h>
#include <stdio.h>

static FT_Library g_ft;
static FT_Face g_face;
static int g_ft_ready = 0;

static const char* k_fonts[] = {
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    0
};

int gui_measure_hook(int px) {
    if (!g_ft_ready) {
        if (FT_Init_FreeType(&g_ft) != 0) { return -1; }
        g_ft_ready = 1;
    }
    for (int i = 0; k_fonts[i]; i++) {
        if (FT_New_Face(g_ft, k_fonts[i], 0, &g_face) == 0) {
            FT_Set_Pixel_Sizes(g_face, 0, (FT_UInt)px);
            return 0;
        }
    }
    return -1;
}

static unsigned long utf8_next(const unsigned char* s, int* i) {
    unsigned char c = s[*i];
    if (c < 0x80) { (*i)++; return c; }
    int n = (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    unsigned long cp = (unsigned long)(c & (unsigned char)(0xFF >> (n + 1)));
    int k = 1;
    while (k < n) {
        cp = (cp << 6) | (unsigned long)(s[*i + k] & 0x3F);
        k += 1;
    }
    *i += n;
    return cp;
}

static int ft_width(const char* s, int size) {
    if (!g_face) { return (int)(strlen(s) * size * 0.55f); }
    int w = 0;
    for (int i = 0; s[i]; ) {
        unsigned long cp = utf8_next((const unsigned char*)s, &i);
        FT_UInt gi = FT_Get_Char_Index(g_face, cp);
        if (gi == 0 || FT_Load_Glyph(g_face, gi, FT_LOAD_DEFAULT) != 0) {
            w += (int)(size * 0.6f);
            continue;
        }
        w += (int)(g_face->glyph->advance.x >> 6);
    }
    return w;
}

int gui_ft_measure(const char* s, int size) {
    return ft_width(s, size);
}

static Clay_Dimensions ft_measure(Clay_StringSlice text, Clay_TextElementConfig* cfg, void* ud) {
    (void)ud;
    char buf[256];
    int n = text.length < 255 ? text.length : 255;
    if (n < 0) { n = 0; }
    memcpy(buf, text.chars, (size_t)n);
    buf[n] = 0;
    return (Clay_Dimensions){ (float)ft_width(buf, cfg->fontSize),
                              (float)cfg->fontSize * 1.25f };
}

int gui_measure_install(void) {
    Clay_SetMeasureTextFunction(ft_measure, 0);
    return 0;
}
