// ft_shim.c —— S9:FreeType 中文纹理 + 矩形绘制(M3 窗口口;§12.3d 同口)
// 纹理注册表:ft_render → ft_tex_upload(id) → ft_tex_draw(id,x,y)
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

int ft_render(const char* utf8, int r, int g, int b) {
    if (!g_face) { return -1; }
    int len = (int)strlen(utf8);
    int asc = g_face->size->metrics.ascender >> 6;
    int hgt = (int)((g_face->size->metrics.height >> 6)) + 4;
    int i = 0;
    int pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next((const unsigned char*)utf8, &i);
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        pen += (int)(g_face->glyph->advance.x >> 6);
    }
    g_bw = pen + 8;
    g_bh = hgt;
    g_buf = (unsigned char*)realloc(g_buf, (size_t)g_bw * g_bh * 4);
    memset(g_buf, 0, (size_t)g_bw * g_bh * 4);
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
                unsigned char* q = &g_buf[((size_t)py * g_bw + px) * 4];
                q[0] = (unsigned char)r; q[1] = (unsigned char)g; q[2] = (unsigned char)b;
                if (a > q[3]) { q[3] = a; }
            }
        }
        pen += (int)(sl->advance.x >> 6);
    }
    return 0;
}

// ---- 纹理注册表(id → Texture2D;ft_render 后需 ft_tex_upload 刷新) ----
static Texture2D g_texs[8];
static int g_texv[8];

int ft_tex_upload(int id) {
    if (id < 0 || id >= 8) { return -1; }
    if (g_texv[id]) { UnloadTexture(g_texs[id]); }
    Image img = { 0 };
    img.data = g_buf;
    img.width = g_bw;
    img.height = g_bh;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    g_texs[id] = LoadTextureFromImage(img);
    g_texv[id] = 1;
    return 0;
}

int ft_tex_draw(int id, int x, int y) {
    if (id < 0 || id >= 8 || !g_texv[id]) { return -1; }
    DrawTexture(g_texs[id], x, y, WHITE);
    return 0;
}

int gui_clear(int r, int g, int b, int a) {
    ClearBackground((Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a });
    return 0;
}

// 矩形(圆角;demo 按钮/面板用)
int gui_rect(int x, int y, int w, int h, int rr, int r, int g, int b) {
    DrawRectangleRounded(
        (Rectangle){ (float)x, (float)y, (float)w, (float)h },
        (float)rr / 100.0f, 8,
        (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    return 0;
}
