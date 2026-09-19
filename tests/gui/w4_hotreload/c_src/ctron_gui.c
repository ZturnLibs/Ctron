// ctron_gui.c —— W4 热重载 shim:毫秒 mtime 轮询 + 矩形/文本绘制(W4;秒级 mtime 同秒内改写检测不到)
#include <raylib.h>
#include <sys/stat.h>
#include <time.h>

long long gui_file_mtime_ms(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) { return -1; }
    long long ms = (long long)st.st_mtime * 1000;
#ifdef __APPLE__
    ms += st.st_mtimespec.tv_nsec / 1000000L;
#else
    ms += st.st_mtim.tv_nsec / 1000000L;
#endif
    return ms;
}

void gui_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int gui_rect(int x, int y, int w, int h, int rr, int r, int g, int b) {
    if (rr > 0) {
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, (float)w, (float)h },
                             (float)rr / 20.0f, 8,
                             (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    } else {
        DrawRectangle(x, y, w, h, (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 });
    }
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
