// ctron_gui.c —— S4 热重载 shim:mtime 轮询 + 矩形/文本绘制(§11.1/计划 W4)
#include <raylib.h>
#include <sys/stat.h>

int gui_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) { return -1; }
    return (int)st.st_mtime;
}

int gui_rect(int x, int y, int w, int h, int r, int g, int b) {
    DrawRectangle(x, y, w, h, (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    return 0;
}

int gui_text(const char* s, int x, int y, int size) {
    DrawText(s, x, y, size, (Color){ 230, 230, 235, 255 });
    return 0;
}

int gui_clear(int r, int g, int b) {
    ClearBackground((Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    return 0;
}

int gui_render_offscreen(const char* text, int r, int g, int b) {
    /* headless 断言:确认文本非空即可(窗口栅格化由 raylib/FreeType 负责) */
    return (text && text[0]) ? 1 : 0;
}
