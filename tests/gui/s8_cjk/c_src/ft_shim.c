// ft_shim.c —— M3 文本管线第一片(规范 §12.1/§6.4):FreeType UTF-8 行渲染 → RGBA buffer
// 头less 断言口:位图非空/宽度;窗口口:ft_tex_draw(LoadTextureFromImage 缓存 + DrawTexture)
// 字体:平台 CJK 路径列表逐一尝试(macOS Hiragino/PingFang;Linux Noto/WQY)
#include <ft2build.h>
#include FT_FREETYPE_H
#include <raylib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static FT_Library g_ft;
static FT_Face g_face;
static int g_ft_ready = 0;

static unsigned char* g_buf = 0;
static int g_bw = 0;
static int g_bh = 0;
static Texture2D g_tex;
static int g_tex_valid = 0;

static const char* k_fonts[] = {
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Supplemental/Songti.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    0
};

int ft_load_cjk(int px) {
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

// UTF-8 → 码点(1–4 字节);游标前移
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

static int g_fr = 230;
static int g_fg = 230;
static int g_fb = 235;

// 渲染 UTF-8 行 → RGBA(白底字形 × 指定色);测量+合成两遍
int ft_render(const char* utf8, int r, int g, int b) {
    if (!g_face) { return -1; }
    g_fr = r; g_fg = g; g_fb = b;
    int len = (int)strlen(utf8);

    // 测量
    int pen = 0;
    int asc = g_face->size->metrics.ascender >> 6;
    int hgt = (int)((g_face->size->metrics.height >> 6)) + 4;
    int i = 0;
    int maxr = asc + 2;
    while (i < len) {
        unsigned long cp = utf8_next((const unsigned char*)utf8, &i);
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        pen += (int)(g_face->glyph->advance.x >> 6);
        int top = asc - (g_face->glyph->metrics.height >> 6);
        if (top < maxr) { maxr = top; }
    }
    (void)maxr;
    g_bw = pen + 8;
    g_bh = hgt;
    g_buf = (unsigned char*)realloc(g_buf, (size_t)g_bw * g_bh * 4);
    memset(g_buf, 0, (size_t)g_bw * g_bh * 4);

    // 合成
    i = 0;
    pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next((const unsigned char*)utf8, &i);
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_RENDER) != 0) { continue; }
        FT_GlyphSlot sl = g_face->glyph;
        int bx = pen + sl->bitmap_left;
        int by = asc - sl->bitmap_top;
        int bw = (int)sl->bitmap.width;
        int bh = (int)sl->bitmap.rows;
        for (int gy = 0; gy < bh; gy++) {
            for (int gx = 0; gx < bw; gx++) {
                int px = bx + gx;
                int py = by + gy;
                if (px < 0 || py < 0 || px >= g_bw || py >= g_bh) { continue; }
                unsigned char a = sl->bitmap.buffer[(size_t)gy * bw + gx];
                unsigned char* p = &g_buf[((size_t)py * g_bw + px) * 4];
                p[0] = (unsigned char)r; p[1] = (unsigned char)g; p[2] = (unsigned char)b;
                if (a > p[3]) { p[3] = a; }
            }
        }
        pen += (int)(sl->advance.x >> 6);
    }
    if (g_tex_valid) { UnloadTexture(g_tex); g_tex_valid = 0; }
    return 0;
}

int ft_buf_w(void) { return g_bw; }
int ft_buf_h(void) { return g_bh; }
// headless 断言口:任意非透明像素即 1
int ft_buf_nonempty(void) {
    size_t n = (size_t)g_bw * g_bh * 4;
    for (size_t k = 3; k < n; k += 4) {
        if (g_buf[k] > 0) { return 1; }
    }
    return 0;
}

// 窗口口:把当前 buffer 画到 (x,y)(首次调用上传纹理)
void ft_tex_draw(int x, int y) {
    if (!g_tex_valid) {
        Image img = { 0 };
        img.data = g_buf;
        img.width = g_bw;
        img.height = g_bh;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        g_tex = LoadTextureFromImage(img);
        g_tex_valid = 1;
    }
    DrawTexture(g_tex, x, y, WHITE);
}
