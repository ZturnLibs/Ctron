// ctron_gui.c —— S5:布局桥(承 S3/S4;gui_cfg 改收 bg 打包色 0x00RRGGBB,0=无背景)
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Clay_RenderCommandArray g_cmds = { 0 };

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
int gui_cmd_bg_r(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.r; }
int gui_cmd_bg_g(int i) { return (int)cmd(i)->renderData.rectangle.backgroundColor.g; }
