// reload_util.c —— w4_ctml_reload 夹具本地:毫秒 mtime / sleep / 就绪 touch
// 命名 w4r_ 前缀防与共享 std/gui 撞车(gui_clear 已在 std/gui/c_src/ctron_gui.c 定义,不重复)
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>

long long w4r_mtime_ms(const char* path) {
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

void w4r_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void w4r_touch(const char* path) {
    if (path && path[0]) {
        FILE* f = fopen(path, "w");
        if (f) { fclose(f); }
    }
}
