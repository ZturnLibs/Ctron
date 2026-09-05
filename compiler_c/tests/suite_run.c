// suite_run.c —— C5a 自举入口验收:运行 selfhost/*.ct 的 fn main,核对退出码/输出。
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "rt.h"

static char* read_file_str(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "../selfhosted";
    DIR* d = opendir(dir);
    if (!d) { fprintf(stderr, "suite_run: 无法打开 %s\n", dir); return 2; }
    size_t n = 0, fails = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type != DT_REG) continue;
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        if (strncmp(e->d_name, "input_", 6) == 0) continue; // 数据夹具跳过
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        char* src = read_file_str(path);
        if (!src) { fprintf(stderr, "无法读取 %s\n", path); fails++; continue; }
        if (!strstr(src, "fn main")) { free(src); continue; } // 数据文件跳过
        ctron_parse_result pr = ctron_parse_src(src, strlen(src));
        free(src);
        if (pr.ndiags) { fprintf(stderr, "%s: 解析诊断\n", e->d_name); ctron_parse_result_free(&pr); fails++; continue; }
        rt_run rr = ctron_rt_run_main(pr.file);
        if (rr.st == RT_OK && rr.exit_code == 0) n++;
        else {
            fprintf(stderr, "%s: 运行失败 st=%d rc=%ld out=%s msg=%s\n", e->d_name, rr.st,
                    rr.exit_code, rr.out ? rr.out : "", rr.msg ? rr.msg : "");
            fails++;
        }
        ctron_rt_run_free(&rr);
        ctron_parse_result_free(&pr);
    }
    closedir(d);
    printf("suite_run: %zu files(pass), %zu failures\n", n, fails);
    return fails ? 1 : 0;
}
