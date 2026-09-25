// ft_shim.c —— GUI 域库单一真源:FreeType 中文纹理 + 矩形绘制(§1.1 目标 7①)
// W6 换面(2026-09-19):本文件 = s9 全集 ∪ headless 探针(ft_last_w/ft_last_h/
// ft_probe_nonzero,s8 的 ft_buf_* 改名承接);阶梯修复时在此唯一落点同步。
// M3 合流(2026-09-23):新增实测测量(gui_ft_measure*)+ 字符串纹理 LRU 缓存
// (gui_ft_text*,flush 口径,ctron_gui.c 调用);直绘路径 ft_render/ft_tex_* 语义零变化。
// 纹理注册表:ft_render → ft_tex_upload(id) → ft_tex_draw(id,x,y)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <raylib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static FT_Library g_ft;
static FT_Face g_face;
static int g_ft_ready = 0;
static int g_ft_px = 0;

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

// 定长 UTF-8 步进(Clay StringSlice 非 NUL 结尾,必须按 len 扫)
static unsigned long utf8_next_n(const unsigned char* s, int len, int* i) {
    if (*i >= len) { return 0; }
    unsigned char c = s[*i];
    if (c < 0x80) { (*i)++; return c; }
    int n = (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    if (*i + n > len) { *i = len; return 0; }
    unsigned long cp = (unsigned long)(c & (unsigned char)(0xFF >> (n + 1)));
    int k = 1;
    while (k < n) {
        cp = (cp << 6) | (unsigned long)(s[*i + k] & 0x3F);
        k += 1;
    }
    *i += n;
    return cp;
}

// 两遍渲染核心(测宽 → 画);定长口径,ft_render/缓存路径共用
// weight≥600 = 合成加粗(§2.8):outline Embolden + advance 增量 px/24(测宽轮同加,缓冲不溢)
static int ft_render_n_w(const char* utf8, int len, int r, int g, int b, int weight) {
    if (!g_face) { return -1; }
    int bold = (weight >= 600) ? 1 : 0;
    int bdelta = bold ? (g_ft_px / 24 + 1) : 0;
    int asc = g_face->size->metrics.ascender >> 6;
    int hgt = (int)((g_face->size->metrics.height >> 6)) + 4;
    int i = 0;
    int pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)utf8, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        pen += (int)(g_face->glyph->advance.x >> 6) + bdelta;
    }
    g_bw = pen + 8;
    g_bh = hgt;
    g_buf = (unsigned char*)realloc(g_buf, (size_t)g_bw * g_bh * 4);
    memset(g_buf, 0, (size_t)g_bw * g_bh * 4);
    i = 0;
    pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)utf8, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_RENDER) != 0) { continue; }
        FT_GlyphSlot sl = g_face->glyph;
        if (bold) { FT_Outline_Embolden(&sl->outline, (FT_Pos)(g_ft_px / 16 + 1)); }
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
        pen += (int)(sl->advance.x >> 6) + bdelta;
    }
    return 0;
}

static int ft_render_n(const char* utf8, int len, int r, int g, int b) {
    return ft_render_n_w(utf8, len, r, g, b, 400);
}

int ft_render(const char* utf8, int r, int g, int b) {
    return ft_render_n(utf8, (int)strlen(utf8), r, g, b);
}

// ---- headless 探针:最近一次渲染的缓冲读出口(gui_ft_text miss 亦写 g_buf,探针通用) ----
int ft_last_w(void) { return g_bw; }
int ft_last_h(void) { return g_bh; }
// 非零 alpha 像素计数(位图非空断言;对齐 s8 口径)
int ft_probe_nonzero(void) {
    if (!g_buf) { return 0; }
    int n = 0;
    long total = (long)g_bw * g_bh;
    for (long i = 0; i < total; i++) { if (g_buf[i * 4 + 3]) { n++; } }
    return n;
}

// ---- M3 合流:实测测量 + 字符串纹理 LRU 缓存(flush 口径) ----
// 白色 RGBA 入缓存,颜色在 gui_ft_text_draw 用 tint 上色——缓存键 = (串,px)。
// 启发式回退(CTRON_GUI_FT_OFF=1 / 无字体)在 ctron_gui.c 的 ctron_measure 侧,本文件不读 env。
static int g_ft_failed = 0;

// 懒加载 + 按需换字号(face 全局单例,测量/渲染/直绘共享,入口各自先设 px)
static int ft_ensure(int px) {
    if (g_ft_failed) { return -1; }
    if (!g_face) {
        if (ft_load_cjk(24) != 0) { g_ft_failed = 1; return -1; }
        g_ft_px = 24;
    }
    if (px > 0 && px != g_ft_px) {
        FT_Set_Pixel_Sizes(g_face, 0, (FT_UInt)px);
        g_ft_px = px;
    }
    return 0;
}

// 实测宽:与 ft_render_n_w 同一迭代+同 bold 增量,保证测量==渲染完全一致
static int ft_measure_n_w(const char* s, int len, int px, int weight) {
    if (ft_ensure(px) != 0) { return -1; }
    int bdelta = (weight >= 600) ? (g_ft_px / 24 + 1) : 0;
    int w = 0;
    int i = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)s, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        w += (int)(g_face->glyph->advance.x >> 6) + bdelta;
    }
    return w;
}
static int ft_measure_n(const char* s, int len, int px) {
    return ft_measure_n_w(s, len, px, 400);
}
int gui_ft_measure(const char* s, int px) {
    return ft_measure_n(s, (int)strlen(s), px);
}
int gui_ft_measure_n(const char* s, int len, int px) {
    return ft_measure_n(s, len, px);
}
int gui_ft_measure_n_wt(const char* s, int len, int px, int weight) {
    return ft_measure_n_w(s, len, px, weight);
}

#define FT_TEXT_CACHE 64
typedef struct {
    char* str;
    int len;
    int px;
    int weight;
    unsigned char* buf;
    int w;
    int h;
    Texture2D tex;
    int uploaded;
    long use;
} FtText;
static FtText g_tc[FT_TEXT_CACHE];
static int g_tc_n = 0;
static long g_tc_clock = 0;

// 缓存查找/渲染,返回槽位;miss 时白色渲染进 g_buf(探针可读)后拷入槽位
// 缓存键 = (串,px,weight) 三元(§2.8);wt 后缀避让 gui_ft_text_w(slot) 旧槽宽读面
int gui_ft_text_wt(const char* s, int len, int px, int weight) {
    if (ft_ensure(px) != 0) { return -1; }
    g_tc_clock++;
    for (int i = 0; i < g_tc_n; i++) {
        if (g_tc[i].px == px && g_tc[i].weight == weight && g_tc[i].len == len &&
            memcmp(g_tc[i].str, s, (size_t)len) == 0) {
            g_tc[i].use = g_tc_clock;
            return i;
        }
    }
    if (ft_render_n_w(s, len, 255, 255, 255, weight) != 0) { return -1; }
    int slot;
    if (g_tc_n < FT_TEXT_CACHE) {
        slot = g_tc_n++;
    } else {
        slot = 0;
        for (int i = 1; i < FT_TEXT_CACHE; i++) {
            if (g_tc[i].use < g_tc[slot].use) { slot = i; }
        }
        if (g_tc[slot].uploaded) { UnloadTexture(g_tc[slot].tex); g_tc[slot].uploaded = 0; }
        free(g_tc[slot].str);
        free(g_tc[slot].buf);
    }
    FtText* e = &g_tc[slot];
    e->len = len;
    e->px = px;
    e->weight = weight;
    e->str = (char*)malloc((size_t)len);
    memcpy(e->str, s, (size_t)len);
    e->w = g_bw;
    e->h = g_bh;
    e->buf = (unsigned char*)malloc((size_t)g_bw * g_bh * 4);
    memcpy(e->buf, g_buf, (size_t)g_bw * g_bh * 4);
    e->uploaded = 0;
    e->use = g_tc_clock;
    return slot;
}

int gui_ft_text(const char* s, int len, int px) {
    return gui_ft_text_wt(s, len, px, 400);
}

// 槽宽读面(s28 夹具消费)
int gui_ft_text_w(int slot) {
    if (slot < 0 || slot >= g_tc_n) { return -1; }
    return g_tc[slot].w;
}

// NUL 串 2 参包装(FFI 口,s28/.ct 测试用)——定长 3 参版仅供 C 侧 flush(Clay 切片非 NUL 结尾,
// 形参错位 = .ct 声明 2 参 / C 定义 3 参的 ABI 教训,与 ARM64 ≥9 参同族)
int gui_ft_text_str(const char* s, int px) {
    return gui_ft_text(s, (int)strlen(s), px);
}

// 懒上传 + tint 绘制(GL;仅窗口口径 flush 调用)
int gui_ft_text_draw(int slot, int x, int y, int r, int g, int b, int a) {
    if (slot < 0 || slot >= g_tc_n) { return -1; }
    FtText* e = &g_tc[slot];
    if (!e->uploaded) {
        Image img = { 0 };
        img.data = e->buf;
        img.width = e->w;
        img.height = e->h;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        e->tex = LoadTextureFromImage(img);
        e->uploaded = 1;
    }
    DrawTexture(e->tex, x, y, (Color){ (unsigned char)r, (unsigned char)g,
                                       (unsigned char)b, (unsigned char)a });
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

// M3 合流改名:原 gui_clear 与 ctron_gui.c 的 3 参 gui_clear 撞符号(此前无夹具同链两文件,
// 链接面合流后暴露)——直绘路径改 ft_gui_clear
int ft_gui_clear(int r, int g, int b, int a) {
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
