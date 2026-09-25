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

static Clay_Dimensions ctron_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *ud) {
    (void)ud;
    if (gui_ft_off < 0) { gui_ft_off = getenv("CTRON_GUI_FT_OFF") ? 1 : 0; }
    float wf = -1.0f;
    if (!gui_ft_off) {
        int wi = gui_ft_measure_n(text.chars, (int)text.length, (int)cfg->fontSize);
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
                                         (bg_packed == 0) ? (float)g_pending_alpha : (float)g_pending_alpha };
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
    if (gui_trace()) { fprintf(stderr, "T%03d text size=%d len=%zu head=%.24s\n", ++gui_trace_n, size, strlen(s), s); }
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
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_StringSlice s = c->renderData.text.stringContents;
                Clay_Color col = c->renderData.text.textColor;
                int slot = gui_ft_text(s.chars, (int)s.length, (int)c->renderData.text.fontSize);
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
