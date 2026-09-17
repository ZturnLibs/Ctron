// gui_abi.c —— S2 探针:repr(c) struct 按值双向(§3.3-1 GUI 定点)
#include "raylib.h"

Color gui_make_color(int r, int g, int b) {
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

int gui_probe_color(Color c, int r, int g, int b, int a) {
    return (c.r == r && c.g == g && c.b == b && c.a == a) ? 1 : 0;
}

int gui_probe_vector2(Vector2 v, float x, float y) {
    return (v.x == x && v.y == y) ? 1 : 0;
}
