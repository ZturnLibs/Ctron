// ctron_gui.c —— GUI 域库单一真源(§1.1 目标 7①:域库内部固定桥,用户不写不携带)
// W6 换面(2026-09-19):本文件 = s7 全集 ∪ s4(键注入/键读 + GetKeyPressed 合并 poll
// + 背景探针)∪ h100;阶梯修复时在此唯一落点同步,夹具/示例经 run.sh 链接本文件。
// Clay 大 struct 留 C 侧,对 Ctron 只暴露标量窄接口(FFI 指针形参发射面未落地前不变);
// flush 为 §12.3d 唯一 raylib 绘制口。窗口循环下 gui_poll_event 的 raylib 合并路径生效。
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Clay_RenderCommandArray g_cmds = { 0 };

// 8 位色 alpha 通道(§7):gui_alpha 置位 → 下一次 gui_cfg 消费即复位 255(单线程折叠序)
int g_pending_alpha = 255;

// 图像纹理缓存(flush 专用;定义在文件尾,flush 分支前向声明)
static Texture2D *gui_tex_cache_get(const char *path);

// ft 缓存 weight 入口(ft_shim 定义;flush TEXT 分支前向声明)
extern int gui_ft_text_wt(const char* s, int len, int px, int weight, int fam);
extern int gui_ft_measure_n_wt(const char* s, int len, int px, int weight, int fam);

// 字重当前值(§2.8):Clay 文本测量内联于 OpenTextElement 同步发生(每文本两次),
// gui_text_w 先置值再开元素,测量回调直读——无序号算术
int gui_text_w(const char *s, int size, int weight, int r, int g, int b, int a);
static int g_current_weight = 400;
static int *g_text_weights = NULL;
static int g_text_weights_cap = 0;
static int g_text_weights_n = 0;

// ---- 事件注入队列(S4 测试缝;事件码:1=KeyDown 2=Click 3=TextInput) ----
// 容量 256:进程累计、不回卷——gui_calc headless 全场景 ~70 次注入,64 会静默丢尾。
typedef struct { int type; int key; int x; int y; } GuiEvent;
static GuiEvent g_queue[256];
static int g_qhead = 0;
static int g_qtail = 0;
static GuiEvent g_cur = { 0, 0, 0, 0 };

void gui_inject_key(int key) {
    if (g_qtail < 256) { g_queue[g_qtail] = (GuiEvent){ 1, key, 0, 0 }; g_qtail++; }
}
void gui_inject_char(int ch) {
    if (g_qtail < 256) { g_queue[g_qtail] = (GuiEvent){ 3, ch, 0, 0 }; g_qtail++; }
}
void gui_inject_click(int x, int y) {
    if (g_qtail < 256) { g_queue[g_qtail] = (GuiEvent){ 2, 0, x, y }; g_qtail++; }
}
// 取下一事件:注入队列优先,再合并 raylib 轮询(headless 下惰性)
// 事件码:1=KeyDown 2=Click 3=TextInput(§12.3b GetCharPressed → 上屏文本)
int gui_poll_event(void) {
    if (g_qhead < g_qtail) {
        g_cur = g_queue[g_qhead];
        g_qhead++;
        return g_cur.type;
    }
    int c = GetCharPressed();
    if (c > 0) { g_cur = (GuiEvent){ 3, c, 0, 0 }; return 3; }
    int k = GetKeyPressed();
    if (k != 0) { g_cur = (GuiEvent){ 1, k, 0, 0 }; return 1; }
    float wv = GetMouseWheelMove();
    if (wv != 0.0f) {
        int iv = (int)wv;
        g_cur = (GuiEvent){ 4, iv, 0, 0 };
        return 4;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 p = GetMousePosition();
        g_cur = (GuiEvent){ 2, 0, (int)p.x, (int)p.y };
        return 2;
    }
    return 0;
}
int gui_clear(int r, int g, int b) {
    ClearBackground((Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    return 0;
}
int gui_evt_key(void) { return g_cur.key; }
int gui_evt_x(void) { return g_cur.x; }
int gui_evt_y(void) { return g_cur.y; }

// ---- Clay 初始化与布局桥 ----
static void clay_err(Clay_ErrorData data) {
    (void)data;
    fprintf(stderr, "ctron_gui: clay error\n");
    abort();
}

// ft_shim.c(M3 合流):实测测量 + 字符串纹理缓存;链接面须含 ft_shim.c + libfreetype
extern int gui_ft_measure_n(const char* s, int len, int px);
extern int gui_ft_text(const char* s, int len, int px);
extern int gui_ft_text_draw(int slot, int x, int y, int r, int g, int b, int a);

// 测量:FreeType 实测接管;CTRON_GUI_FT_OFF=1 → 0.55 启发式(坐标黄金夹具钉值,跨平台稳定);
// 字体缺失(ft_shim 侧 g_ft_failed)同样回启发式——无 CJK 字体环境行为与旧版一致
static int gui_ft_off = -1;

extern int gui_ft_measure_n_wt(const char* s, int len, int px, int weight, int fam);
static int g_measure_ord = 0;
static Clay_Dimensions ctron_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *ud) {
    (void)ud;
    if (gui_ft_off < 0) { gui_ft_off = getenv("CTRON_GUI_FT_OFF") ? 1 : 0; }
    float wf = -1.0f;
    if (!gui_ft_off) {
        // 字重按序弹(产出序=布局遍历序;gui_begin_layout 复位)
        int wt = g_current_weight;
        int fam2 = wt / 10000;
        int wt2 = wt % 10000;
        int wi = gui_ft_measure_n_wt(text.chars, (int)text.length, (int)cfg->fontSize, wt2, fam2);
        if (wi >= 0) { wf = (float)wi; }
    }
    if (wf < 0.0f) {
        // 启发式回退保持旧式纯浮点(length×size×0.55f 不取整)——s17/s21 等黄金口径按
        // 「×100 恰为 bytes×size×55」断言,(int) 截断即红
        wf = (float)text.length * (float)cfg->fontSize * 0.55f;
    }
    return (Clay_Dimensions){ wf, (float)cfg->fontSize * 1.25f };
}

int gui_clay_init(int w, int h) {
    uint32_t min = Clay_MinMemorySize();
    void *mem = malloc(min);
    Clay_Initialize(Clay_CreateArenaWithCapacityAndMemory(min, mem),
                    (Clay_Dimensions){ (float)w, (float)h },
                    (Clay_ErrorHandler){ clay_err });
    Clay_SetMeasureTextFunction(ctron_measure, 0);
    return (int)min;
}


int gui_begin_layout(int w, int h) {
    g_text_weights_n = 0;
    g_current_weight = 400;    g_text_weights_n = 0;
    Clay_SetLayoutDimensions((Clay_Dimensions){ (float)w, (float)h });
    Clay_BeginLayout();
    return 0;
}
// ---- CTRON_GUI_TRACE=1:Clay 调用序列 trace(stderr 直出;双口径分叉定位用) ----
static int gui_trace_on = -1;
static int gui_trace(void) {
    if (gui_trace_on < 0) { gui_trace_on = getenv("CTRON_GUI_TRACE") ? 1 : 0; }
    return gui_trace_on;
}
static int gui_trace_n = 0;
int gui_open(void) {
    if (gui_trace()) { fprintf(stderr, "T%03d open\n", ++gui_trace_n); }
    Clay__OpenElement();
    return 0;
}
int gui_close(void) {
    if (gui_trace()) { fprintf(stderr, "T%03d close\n", ++gui_trace_n); }
    Clay__CloseElement();
    return 0;
}

static int gui_cfg_impl(int dir, int gap, int padx, int pady, int ax, int ay,
                        int wmode, int wval, int hmode, int hval, int bg_packed, int clipv, int offsetpx) {
    if (gui_trace()) { fprintf(stderr, "T%03d cfg dir=%d gap=%d padx=%d wmode=%d wval=%d hmode=%d hval=%d bg=%06x clip=%d\n", ++gui_trace_n, dir, gap, padx, wmode, wval, hmode, hval, bg_packed, clipv); }
    float wf = (float)wval;
    float hf = (float)hval;
    Clay_LayoutConfig lay = {
        .layoutDirection = (dir == 0) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
        .padding = { .left = (uint16_t)padx, .right = (uint16_t)padx,
                     .top = (uint16_t)pady, .bottom = (uint16_t)pady },
        .childGap = (uint16_t)gap,
        .childAlignment = {
            (ax == 1) ? CLAY_ALIGN_X_CENTER : (ax == 2) ? CLAY_ALIGN_X_RIGHT : CLAY_ALIGN_X_LEFT,
            (ay == 1) ? CLAY_ALIGN_Y_CENTER : (ay == 2) ? CLAY_ALIGN_Y_BOTTOM : CLAY_ALIGN_Y_TOP,
        },
    };
    if (wmode == 1) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (wmode == 2) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { wf, wf } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    if (hmode == 1) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (hmode == 2) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { hf, hf } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    Clay_ElementDeclaration decl = { 0 };
    decl.layout = lay;
    decl.backgroundColor = (Clay_Color){ (float)((bg_packed >> 16) & 255),
                                         (float)((bg_packed >> 8) & 255),
                                         (float)(bg_packed & 255),
                                         (bg_packed == 0 && g_pending_alpha == 255) ? 0.0f : (float)g_pending_alpha };
    g_pending_alpha = 255;
    if (clipv) {
        decl.clip = (Clay_ClipElementConfig){ false, true, { 0.0f, (float)-offsetpx } };
    }
    Clay__ConfigureOpenElement(decl);
    return 0;
}

// 形参 int:与解释桥 int×N cast(栈参 4 字节步长)及 CTron I32 声明三层全 int
// 一致——long 形参 shim 按 8 字节窗口错读 4 字节步长栈参(args11 实证 hval=11)
int gui_cfg(int dir, int gap, int padx, int pady, int ax, int ay,
            int wmode, int wval, int hmode, int hval, int bg_packed) {
    return gui_cfg_impl(dir, gap, padx, pady, ax, ay, wmode, wval, hmode, hval, bg_packed, 0, 0);
}

// 滚动能力(gui_cfg2):clipv = 垂直裁剪/滚动容器(§13 T0 scroll);
// offsetpx = 运行时本地滚动偏移(§12.2;滚轮事件由应用侧累计,Clay 按偏移裁剪)
int gui_cfg2(int dir, int gap, int padx, int pady, int ax, int ay,
             int wmode, int wval, int hmode, int hval, int bg_packed, int clipv, int offsetpx) {
    return gui_cfg_impl(dir, gap, padx, pady, ax, ay, wmode, wval, hmode, hval, bg_packed, clipv, offsetpx);
}

int gui_text(const char *s, int size, int r, int g, int b, int a) {
    return gui_text_w(s, size, 400, r, g, b, a);
}

// 字重文本(§2.8):weight 进影子表(产出序),flush 按 TEXT 序号取回传 ft 缓存
int gui_text_w(const char *s, int size, int weight, int r, int g, int b, int a) {
    if (gui_trace()) { fprintf(stderr, "T%03d text size=%d len=%zu head=%.24s\n", ++gui_trace_n, size, strlen(s), s); }
    g_current_weight = weight;
    if (g_text_weights_n >= g_text_weights_cap) {
        int nc = g_text_weights_cap ? g_text_weights_cap * 2 : 64;
        int *ni = (int *)realloc(g_text_weights, sizeof(int) * (size_t)nc);
        if (ni) { g_text_weights = ni; g_text_weights_cap = nc; }
    }
    if (g_text_weights_n < g_text_weights_cap) {
        g_text_weights[g_text_weights_n] = weight;
    }
    g_text_weights_n++;
    Clay_String str = { true, (int32_t)strlen(s), s };
    Clay_TextElementConfig cfg = {
        .textColor = { (float)r, (float)g, (float)b, (float)a },
        .fontSize = (uint16_t)size,
    };
    Clay__OpenTextElement(str, Clay__StoreTextElementConfig(cfg));
    return 0;
}

int gui_end_layout(void) {
    g_cmds = Clay_EndLayout();
    return (int)g_cmds.length;
}

static Clay_RenderCommand *cmd(int i) { return Clay_RenderCommandArray_Get(&g_cmds, i); }
int gui_cmd_count(void) { return (int)g_cmds.length; }
int gui_cmd_type(int i) {
    switch (cmd(i)->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT:      return 1;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: return 2;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:     return 3;
        default:                                 return 0;
    }
}
int gui_cmd_x100(int i) { return (int)(cmd(i)->boundingBox.x * 100.0f); }
int gui_cmd_y100(int i) { return (int)(cmd(i)->boundingBox.y * 100.0f); }
int gui_cmd_w100(int i) { return (int)(cmd(i)->boundingBox.width * 100.0f); }
int gui_cmd_h100(int i) { return (int)(cmd(i)->boundingBox.height * 100.0f); }
int gui_cmd_text_len(int i) { return (int)cmd(i)->renderData.text.stringContents.length; }
// 绑定文本内容断言:逐字节读回
int gui_cmd_text_byte(int i, int j) {
    Clay_StringSlice s = cmd(i)->renderData.text.stringContents;
    if (j < 0 || j >= s.length) { return -1; }
    return (unsigned char)s.chars[j];
}
// 背景色读回(S4/S5 断言用;Clay_Color 分量 = 0..255 浮点)
int gui_cmd_bg_r(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.r; }
int gui_cmd_bg_g(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.g; }
int gui_cmd_bg_b(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.b; }
int gui_cmd_bg_a(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.a; }

// 8 位色 alpha 通道(§7):gui_alpha 置位 → 下一次 gui_cfg 消费即复位 255(单线程折叠序)
void gui_alpha(int a) { g_pending_alpha = a; }

// ---- 绘制 flush(§12.3d 唯一绘制口;窗口口径) ----
void ctron_gui_flush(void) {
    int text_ord = 0;
    for (int32_t i = 0; i < g_cmds.length; i++) {
        Clay_RenderCommand *c = Clay_RenderCommandArray_Get(&g_cmds, i);
        Clay_BoundingBox b = c->boundingBox;
        switch (c->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_Color col = c->renderData.rectangle.backgroundColor;
                DrawRectangleRounded(
                    (Rectangle){ b.x, b.y, b.width, b.height }, 0.15f, 8,
                    (Color){ (unsigned char)col.r, (unsigned char)col.g,
                             (unsigned char)col.b, (unsigned char)col.a });
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                const char *ipath = (const char *)c->renderData.image.imageData;
                if (ipath != NULL) {
                    Texture2D *tex = gui_tex_cache_get(ipath);
                    if (tex != NULL && tex->id != 0) {
                        Rectangle srcrect = { 0, 0, (float)tex->width, (float)tex->height };
                        DrawTexturePro(*tex, srcrect,
                            (Rectangle){ b.x, b.y, b.width, b.height },
                            (Vector2){ 0, 0 }, 0.0f, WHITE);
                    }
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_StringSlice s = c->renderData.text.stringContents;
                Clay_Color col = c->renderData.text.textColor;
                int combo = 400;
                if (text_ord < g_text_weights_n) { combo = g_text_weights[text_ord]; }
                text_ord++;
                int wt = combo % 10000;
                int fam = combo / 10000;
                int slot = gui_ft_text_wt(s.chars, (int)s.length, (int)c->renderData.text.fontSize, wt, fam);
                if (slot >= 0) {
                    gui_ft_text_draw(slot, (int)b.x, (int)b.y,
                                     (int)col.r, (int)col.g, (int)col.b, (int)col.a);
                } else {
                    // 兜底:无 CJK 字体环境回默认位图字体(ASCII 界面仍可用,即旧观感)
                    DrawTextEx(GetFontDefault(), s.chars, (Vector2){ b.x, b.y },
                               (float)c->renderData.text.fontSize, 0.0f,
                               (Color){ (unsigned char)col.r, (unsigned char)col.g,
                                        (unsigned char)col.b, (unsigned char)col.a });
                }
                break;
            }
            default: break;
        }
    }
}

// ---- 主题令牌槽(波次一 §2.4):25 槽 I32,theme_apply 逐槽写入;
// 样式存储期令牌名折成 "@n" 标记,折叠期 slot_get O(1) 读(两段式,禁逐帧名查表) ----
int g_theme_slots[32];
int g_theme_applied = 0;
void gui_theme_slot(int i, int v) { g_theme_slots[i] = v; g_theme_applied = 1; }
int gui_theme_applied(void) { return g_theme_applied; }
int gui_theme_slot_get(int i) {
    if (i < 0 || i >= 32) { return 0; }
    return g_theme_slots[i];
}

// ---- 平台/明暗探测(§2.5;theme_auto 缺省选择;进程内缓存,探测不可得回退深色) ----
int gui_platform_id(void) {
#if defined(__APPLE__)
    return 0;
#elif defined(_WIN32)
    return 1;
#else
    return 2;
#endif
}
int g_os_dark = -1;
int gui_os_dark_id(void) {
    if (g_os_dark >= 0) { return g_os_dark; }
    int dark = 1;
#ifdef __APPLE__
    FILE *pp = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r");
    if (pp) {
        char buf[64];
        dark = (fgets(buf, sizeof buf, pp) != NULL) ? 1 : 0;
        pclose(pp);
    }
#elif defined(_WIN32)
    FILE *pp = popen("reg query \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\" /v AppsUseLightTheme 2>nul", "r");
    if (pp) {
        char buf[512];
        dark = 1;
        while (fgets(buf, sizeof buf, pp) != NULL) {
            if (strstr(buf, "0x0")) { dark = 1; }
            if (strstr(buf, "0x1")) { dark = 0; }
        }
        pclose(pp);
    }
#else
    FILE *pp = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (pp) {
        char buf[128];
        dark = (fgets(buf, sizeof buf, pp) != NULL && strstr(buf, "dark") != NULL) ? 1 : 0;
        pclose(pp);
    }
#endif
    g_os_dark = dark;
    return dark;
}

// ---- 指针位置/按下读面(§2.1 真窗 hover;headless 走 d_hover/d_active 注入) ----
int gui_mouse_x(void) { Vector2 p = GetMousePosition(); return (int)p.x; }
int gui_mouse_y(void) { Vector2 p = GetMousePosition(); return (int)p.y; }
int gui_mouse_down(void) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { return 1; }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { return 1; }
    return 0;
}

// ---- 焦点/光标/剪贴板/修饰键原语(波次二 §2.2;文本零 C 存储,编辑走 bind 活问+act 载荷) ----
int g_focus_id = -1;
void gui_focus_set(int i) { g_focus_id = i; }
int gui_focus_node(void) { return g_focus_id; }

int g_caret = 0;
void gui_caret_set(int i) { g_caret = i; }
int gui_caret_get(void) { return g_caret; }

char g_clip[4096];
int g_clip_n = 0;
void gui_clip_set_c(const char *s) {
    if (s == NULL) { g_clip_n = 0; return; }
    int i = 0;
    while (s[i] != 0 && i < 4095) { g_clip[i] = s[i]; i++; }
    g_clip[i] = 0;
    g_clip_n = i;
}
int gui_clip_len(void) { return g_clip_n; }
int gui_clip_byte(int i) {
    if (i < 0 || i >= g_clip_n) { return -1; }
    return (unsigned char)g_clip[i];
}

int g_mod_ctrl = 0;
void gui_inject_mod(int m) { g_mod_ctrl = m; }
int gui_mod_ctrl(void) {
    if (g_mod_ctrl > 0) { return 1; }
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) { return 1; }
    return 0;
}

// 剪贴板系统同步(真窗粘贴前调;headless 直控缓冲不经此)
void gui_clip_sync(void) {
    const char *s = GetClipboardText();
    if (s) { gui_clip_set_c(s); }
}

// ---- 浮层容器(§2.3):Clay floating attach ROOT(真全屏,不随父 padding)/zIndex=1/CAPTURE;全屏 grow;
// alignc → childAlignment 居中(卡片作 overlay 子元素自动居中);x/y 偏移(下拉锚定用) ----
int gui_floating(int dir, int gap, int padx, int pady, int ax, int ay,
                 int wmode, int wval, int hmode, int hval, int bg_packed,
                 int alignc, int xoff, int yoff) {
    Clay_LayoutConfig lay = {
        .layoutDirection = (dir == 0) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
        .padding = { .left = (uint16_t)padx, .right = (uint16_t)padx,
                     .top = (uint16_t)pady, .bottom = (uint16_t)pady },
        .childGap = (uint16_t)gap,
    };
    if (alignc) {
        lay.childAlignment = (Clay_ChildAlignment){ CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER };
    }
    lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    Clay_ElementDeclaration decl = { 0 };
    decl.layout = lay;
    decl.backgroundColor = (Clay_Color){ (float)((bg_packed >> 16) & 255),
                                         (float)((bg_packed >> 8) & 255),
                                         (float)(bg_packed & 255),
                                         (float)g_pending_alpha };
    g_pending_alpha = 255;
    decl.floating = (Clay_FloatingElementConfig){
        .offset = { (float)xoff, (float)yoff },
        .zIndex = 1,
        .attachTo = CLAY_ATTACH_TO_ROOT,
        .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
    };
    Clay__ConfigureOpenElement(decl);
    return 0;
}

// ---- 图像/纹理管线(§2.6):命令面只透传路径指针(树内 npre,跨帧稳定);
// LoadTexture 延迟到 flush(真窗独占路径)——headless 无 GL 上下文,加载即崩(实证);
// LRU 16 槽缓存供 flush 复用;缺失路径 → 无 image 配置的底色占位框(fopen 判存在) ----
typedef struct { char path[256]; Texture2D tex; unsigned long long last; } GuiTexSlot;
static GuiTexSlot g_tex[16];
static unsigned long long g_tex_clock = 0;

static Texture2D *gui_tex_cache_get(const char *path) {
    g_tex_clock++;
    int oldest = 0;
    for (int i = 0; i < 16; i++) {
        if (g_tex[i].last == 0) { oldest = i; break; }
        if (g_tex[i].last < g_tex[oldest].last) { oldest = i; }
        if (strncmp(g_tex[i].path, path, 255) == 0) {
            g_tex[i].last = g_tex_clock;
            return &g_tex[i].tex;
        }
    }
    Texture2D t = LoadTexture(path);
    if (t.id == 0) { return NULL; }
    int slot = oldest;
    if (g_tex[slot].tex.id != 0) { UnloadTexture(g_tex[slot].tex); }
    g_tex[slot].tex = t;
    strncpy(g_tex[slot].path, path, 255);
    g_tex[slot].path[255] = 0;
    g_tex[slot].last = g_tex_clock;
    return &g_tex[slot].tex;
}

int gui_image_cfg(const char *path, int wmode, int wval, int hmode, int hval) {
    float wf = (float)wval;
    float hf = (float)hval;
    int exists = 0;
    if (path != NULL) {
        FILE *f = fopen(path, "rb");
        if (f) { exists = 1; fclose(f); }
    }
    Clay_LayoutConfig lay = { 0 };
    if (wmode == 1) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (wmode == 2) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { wf, wf } }, .type = CLAY__SIZING_TYPE_FIXED };
    } else {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { 100, 100 } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    if (hmode == 1) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (hmode == 2) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { hf, hf } }, .type = CLAY__SIZING_TYPE_FIXED };
    } else {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { 100, 100 } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    Clay_ElementDeclaration decl = { 0 };
    decl.layout = lay;
    decl.backgroundColor = (Clay_Color){ 34, 34, 46, 255 };
    if (exists) {
        decl.image = (Clay_ImageElementConfig){ .imageData = (void *)path };
    }
    Clay__ConfigureOpenElement(decl);
    return exists ? 0 : -1;
}

// ---- tick 原语(§2.7):毫秒钟(注入优先,真窗 GetTime)+帧计数;光标闪烁消费口 ----
int g_inject_ms = -1;
void gui_inject_ms(int ms) { g_inject_ms = ms; }
int gui_ms_injected(void) { return g_inject_ms; }
int gui_now_ms(void) {
    if (g_inject_ms >= 0) { return g_inject_ms; }
    return (int)(GetTime() * 1000.0);
}
int g_frame_n = 0;
void gui_frame_tick(void) { g_frame_n++; }
int gui_frame_count(void) { return g_frame_n; }

// ---- 快捷键表(§2.11):16 条目;mods 位 1=ctrl/cmd 2=shift 4=alt;事件链尾 match ----
typedef struct { int mods; int key; char action[64]; } GuiHotkey;
static GuiHotkey g_hotkeys[16];
static int g_nhotkeys = 0;
static int g_hk_last = -1;
void gui_hotkey_set(int mods, int key, const char *action) {
    if (g_nhotkeys >= 16) { return; }
    g_hotkeys[g_nhotkeys].mods = mods;
    g_hotkeys[g_nhotkeys].key = key;
    strncpy(g_hotkeys[g_nhotkeys].action, action ? action : "", 63);
    g_hotkeys[g_nhotkeys].action[63] = 0;
    g_nhotkeys++;
}
int gui_hotkey_mods(void) {
    int m = 0;
    if (gui_mod_ctrl()) { m |= 1; }
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) { m |= 2; }
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) { m |= 4; }
    return m;
}
int gui_hotkey_match(int key, int mods) {
    int i = 0;
    g_hk_last = -1;
    while (i < g_nhotkeys) {
        if (g_hotkeys[i].key == key && g_hotkeys[i].mods == mods) {
            g_hk_last = i;
            return (int)strlen(g_hotkeys[i].action);
        }
        i++;
    }
    return -1;
}
int gui_hotkey_action_byte(int i) {
    if (g_hk_last < 0 || i < 0 || i >= 64) { return -1; }
    return (unsigned char)g_hotkeys[g_hk_last].action[i];
}

// ---- 尺寸约束(§2.10):minmax packed = min*100000+max(上限 max<100000/min<21000);mode 3=GROW{min,max}(max 钳制
// 填充),mode 4=FIT{min,∞}(min 托底 hug 内容)——Clay GROW 不读 max/FIT 不读 min 的实证分工 ----
int gui_cfg3(int dir, int gap, int padx, int pady, int ax, int ay,
             int wmode, int wval, int hmode, int hval, int bg_packed,
             int wminmax, int hminmax) {
    float wf = (float)wval;
    float hf = (float)hval;
    Clay_LayoutConfig lay = {
        .layoutDirection = (dir == 0) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
        .padding = { .left = (uint16_t)padx, .right = (uint16_t)padx,
                     .top = (uint16_t)pady, .bottom = (uint16_t)pady },
        .childGap = (uint16_t)gap,
    };
    if (wmode == 1) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (wmode == 2) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { wf, wf } }, .type = CLAY__SIZING_TYPE_FIXED };
    } else if (wmode == 3) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { (float)(wminmax / 100000), (float)(wminmax % 100000) } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (wmode == 4) {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { (float)(wminmax / 100000), (float)(wminmax % 100000) } }, .type = CLAY__SIZING_TYPE_FIT };
    } else {
        lay.sizing.width = (Clay_SizingAxis){ .size = { .minMax = { wf, wf } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    if (hmode == 1) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { 0, 0 } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (hmode == 2) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { hf, hf } }, .type = CLAY__SIZING_TYPE_FIXED };
    } else if (hmode == 3) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { (float)(hminmax / 100000), (float)(hminmax % 100000) } }, .type = CLAY__SIZING_TYPE_GROW };
    } else if (hmode == 4) {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { (float)(hminmax / 100000), (float)(hminmax % 100000) } }, .type = CLAY__SIZING_TYPE_FIT };
    } else {
        lay.sizing.height = (Clay_SizingAxis){ .size = { .minMax = { hf, hf } }, .type = CLAY__SIZING_TYPE_FIXED };
    }
    Clay_ElementDeclaration decl = { 0 };
    decl.layout = lay;
    decl.backgroundColor = (Clay_Color){ (float)((bg_packed >> 16) & 255),
                                         (float)((bg_packed >> 8) & 255),
                                         (float)(bg_packed & 255),
                                         (float)g_pending_alpha };
    g_pending_alpha = 255;
    Clay__ConfigureOpenElement(decl);
    return 0;
}

// 列表 NULL 安全计数(编译构建树 gui_sk_load 不含波次四新字段时为 NULL;
// Ctron 侧不可判空,经此助手门控——vreg/env 读取前置条件)
typedef struct { unsigned long long magic; char **items; int n; int cap; } CtronListC;
int gui_list_ns(const void *l) {
    if (l == NULL) { return 0; }
    return ((const CtronListC *)l)->n;
}
int gui_list_ni(const void *l) {
    if (l == NULL) { return 0; }
    return ((const CtronListC *)l)->n;
}

// 光标形状(P2):真窗逐帧设定;headless 无窗安全(raylib 全局态直设)
void gui_cursor_set(int shape) { SetMouseCursor(shape); }
