// gui_shim.c —— S1 测试缝与标量封装(规范 §12.3d 前身;§11.2 shim 拥有全部 C 库复杂度)
#include "raylib.h"

int gui_clear(int r, int g, int b) {
    ClearBackground((Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    return 0;
}

int gui_draw_text(const char* s, int x, int y, int size) {
    DrawText(s, x, y, size, (Color){ 230, 230, 235, 255 });
    return 0;
}

int gui_frame_count(void) {
    static int n = 0;
    return ++n;
}
