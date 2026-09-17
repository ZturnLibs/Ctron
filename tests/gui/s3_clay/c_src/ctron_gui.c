// ctron_gui.c —— S3 布局桥 + 命令读回 + 绘制 flush(规范 §11.2/§12.3c/§12.3d)
// 原则:Clay 全部大 struct(ElementDeclaration/SizingAxis/…)留在 C 侧,Ctron 只见标量;
//       命令类型码为 shim 自有稳定码(1=TEXT 2=RECTANGLE),与 Clay 枚举解耦;
//       文本测量用确定性启发式(S3 headless 口径,§12.3c 的 measure 桥签名不变,
//       S6 接窗口后换 raylib 字体测量)。
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Clay_RenderCommandArray g_cmds = { 0 };
static int g_cmd_count = 0;

static void clay_err(Clay_ErrorData data) {
    (void)data;
    fprintf(stderr, "ctron_gui: clay error\n");
    abort();
}

// 确定性测量:宽 = 字符数 × 字号 × 0.55,高 = 字号 × 1.25(黄金断言依赖此确定性)
static Clay_Dimensions ctron_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *ud) {
    (void)ud;
    float w = (float)text.length * (float)cfg->fontSize * 0.55f;
    float h = (float)cfg->fontSize * 1.25f;
    return (Clay_Dimensions){ w, h };
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

// gui_cfg —— 对已打开元素应用布局+背景(码表):
//   dir: 0=左右 1=上下;ax/ay: 0=起 1=中 2=末;wmode/hmode: 0=fit 1=grow 2=fixed(wval/hval 为像素)
//   rr/gg/bb/aa: 背景色(aa=0 即无背景)
// 注意:S3 阶段 shim 边界只用 I32/Str——发射面裸 F32 参数/返回有缺口(位型/清零),
//       修复前 fixed 尺寸以整数像素过桥(计划风险表已登记)
int gui_cfg(int dir, int gap, int padx, int pady, int ax, int ay,
            int wmode, int wval, int hmode, int hval,
            int rr, int gg, int bb, int aa) {
    float wf = (float)wval;
    float hf = (float)hval;
    Clay_LayoutConfig lay = {
        .layoutDirection = (dir == 0) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
        // v0.14 Clay_Padding 字段序 = {left, right, top, bottom}——勿用位置初始化(踩过:top 落 0)
        .padding = { .left = (uint16_t)padx, .right = (uint16_t)padx,
                     .top = (uint16_t)pady, .bottom = (uint16_t)pady },
        .childGap = (uint16_t)gap,
        .childAlignment = {
            (ax == 1) ? CLAY_ALIGN_X_CENTER : (ax == 2) ? CLAY_ALIGN_X_RIGHT : CLAY_ALIGN_X_LEFT,
            (ay == 1) ? CLAY_ALIGN_Y_CENTER : (ay == 2) ? CLAY_ALIGN_Y_BOTTOM : CLAY_ALIGN_Y_TOP,
        },
    };
    Clay_ElementDeclaration decl = { 0 };
    decl.layout = lay;
    // v0.14:sizing 并入 Clay_LayoutConfig(§12.4:版本钉 v0.14,按钉死版本 API 走)
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
    decl.layout = lay;
    decl.backgroundColor = (Clay_Color){ (float)rr, (float)gg, (float)bb, (float)aa };
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
    g_cmd_count = (int)g_cmds.length;
    return g_cmd_count;
}

// ---- 命令读回(测试缝;§11.7 黄金快照的数据源) ----
int gui_cmd_count(void) { return g_cmd_count; }

static Clay_RenderCommand *cmd(int i) { return Clay_RenderCommandArray_Get(&g_cmds, i); }

int gui_cmd_type(int i) {
    switch (cmd(i)->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT:      return 1;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: return 2;
        default:                                 return 0;
    }
}
// 几何读回一律 ×100 整数返回——发射面 extern 标量浮点返回当前有缺口
// (位型直通,见计划风险表;修复后换回浮点口径)
float gui_cmd_x(int i) { return cmd(i)->boundingBox.x; }
float gui_cmd_y(int i) { return cmd(i)->boundingBox.y; }
float gui_cmd_w(int i) { return cmd(i)->boundingBox.width; }
float gui_cmd_h(int i) { return cmd(i)->boundingBox.height; }
int gui_cmd_x100(int i) { return (int)(cmd(i)->boundingBox.x * 100.0f); }
int gui_cmd_y100(int i) { return (int)(cmd(i)->boundingBox.y * 100.0f); }
int gui_cmd_w100(int i) { return (int)(cmd(i)->boundingBox.width * 100.0f); }
int gui_cmd_h100(int i) { return (int)(cmd(i)->boundingBox.height * 100.0f); }
int gui_cmd_text_len(int i) { return (int)cmd(i)->renderData.text.stringContents.length; }

// ---- 绘制 flush(§12.3d:唯一 raylib 绘制口;窗口口径,交互验收用) ----
void ctron_gui_flush(void) {
    for (int32_t i = 0; i < g_cmds.length; i++) {
        Clay_RenderCommand *c = Clay_RenderCommandArray_Get(&g_cmds, i);
        Clay_BoundingBox b = c->boundingBox;
        switch (c->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_Color col = c->renderData.rectangle.backgroundColor;
                DrawRectangleRounded(
                    (Rectangle){ b.x, b.y, b.width, b.height },
                    0.15f, 8,
                    (Color){ (unsigned char)col.r, (unsigned char)col.g,
                             (unsigned char)col.b, (unsigned char)col.a });
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_StringSlice s = c->renderData.text.stringContents;
                Clay_Color col = c->renderData.text.textColor;
                DrawTextEx(GetFontDefault(), s.chars,
                           (Vector2){ b.x, b.y },
                           (float)c->renderData.text.fontSize, 0.0f,
                           (Color){ (unsigned char)col.r, (unsigned char)col.g,
                                    (unsigned char)col.b, (unsigned char)col.a });
                break;
            }
            default: break;
        }
    }
}
