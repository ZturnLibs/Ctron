// ctron_gui.c —— S7:窗口交互 demo shim(S6 全集 + gui_clear;§12.3a/§12.3d)
// 窗口循环下 gui_poll_event 的 raylib 合并路径生效(真实鼠标点击)
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Clay_RenderCommandArray g_cmds = { 0 };

// ---- 事件注入队列(S4 测试缝) ----
typedef struct { int type; int key; int x; int y; } GuiEvent;
static GuiEvent g_queue[64];
static int g_qhead = 0;
static int g_qtail = 0;
static GuiEvent g_cur = { 0, 0, 0, 0 };

void gui_inject_click(int x, int y) {
    if (g_qtail < 64) { g_queue[g_qtail] = (GuiEvent){ 2, 0, x, y }; g_qtail++; }
}
int gui_poll_event(void) {
    if (g_qhead < g_qtail) {
        g_cur = g_queue[g_qhead];
        g_qhead++;
        return g_cur.type;
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
int gui_evt_x(void) { return g_cur.x; }
int gui_evt_y(void) { return g_cur.y; }

// ---- Clay 初始化与布局桥 ----
static void clay_err(Clay_ErrorData data) {
    (void)data;
    fprintf(stderr, "ctron_gui: clay error\n");
    abort();
}

static Clay_Dimensions ctron_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *ud) {
    (void)ud;
    return (Clay_Dimensions){ (float)text.length * (float)cfg->fontSize * 0.55f,
                              (float)cfg->fontSize * 1.25f };
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
int gui_open(void) { Clay__OpenElement(); return 0; }
int gui_close(void) { Clay__CloseElement(); return 0; }

int gui_cfg(int dir, int gap, int padx, int pady, int ax, int ay,
            int wmode, int wval, int hmode, int hval, int bg_packed) {
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
                                         (bg_packed == 0) ? 0.0f : 255.0f };
    Clay__ConfigureOpenElement(decl);
    return 0;
}

int gui_text(const char *s, int size, int r, int g, int b, int a) {
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
int gui_cmd_text_len(int i) { return (int)cmd(i)->renderData.text.stringContents.length; }
// 绑定文本内容断言:逐字节读回
int gui_cmd_text_byte(int i, int j) {
    Clay_StringSlice s = cmd(i)->renderData.text.stringContents;
    if (j < 0 || j >= s.length) { return -1; }
    return (unsigned char)s.chars[j];
}

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
                DrawTextEx(GetFontDefault(), s.chars, (Vector2){ b.x, b.y },
                           (float)c->renderData.text.fontSize, 0.0f,
                           (Color){ (unsigned char)col.r, (unsigned char)col.g,
                                    (unsigned char)col.b, (unsigned char)col.a });
                break;
            }
            default: break;
        }
    }
}
