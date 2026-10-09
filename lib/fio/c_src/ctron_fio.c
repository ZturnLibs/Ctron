// ctron_fio —— 文件字节平面 IO 垫片(lib/fio;CW-F3a 2026-10-09)
// 定位:语言内建 fs 面(Str 域:fs_write/read_file_bytes 槽读族)之下的二进制载荷通道——
//       U8[] 视图直读直写 + 显式逻辑长度,append/pread/排他创建/尺寸探测/getcwd。
// 来源:port 自 loom c_src/loom_raw.c fs 族(loom C 垫片内化战役批3;原 F17 字节平面闭环件)。
// ABI 纪律(ctron_net/ctron_crypto 同款):ctron_view_w8u 与发射器模板逐字段一致
// ({ uint8_t* d; int64_t n; } 形——见下 typedef),数组实参按值传结构体;Str 实参 =
// GC NUL 结尾串,长度一律显式传参不经 strlen;视图 n 是声明容量,逻辑长度由调用方显式传。
// 语义分工:读 = 视图容量即上限(返回实读数);写 = 调用方显式 n。
// rc 口径:写族 1/0;读族 ≥0 字节数 / 负值败(-1 不存在或失败;read_after_line 另有
// -2 无首行 / -3 临时分配失败);write_excl_0600 = 1 新建 / 2 已存在不覆盖 / 0 失败。
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct { uint8_t* d; int64_t n; } ctron_view_w8u;

int32_t ctron_fio_write(const char* path, ctron_view_w8u data, int64_t n) {
    FILE* f = fopen(path, "wb");
    if (!f) { return 0; }
    size_t w = (n > 0) ? fwrite(data.d, 1, (size_t)n, f) : 0;
    fclose(f);
    return (w == (size_t)(n > 0 ? n : 0)) ? 1 : 0;
}

int32_t ctron_fio_append(const char* path, ctron_view_w8u data, int64_t n) {
    FILE* f = fopen(path, "ab");
    if (!f) { return 0; }
    size_t w = (n > 0) ? fwrite(data.d, 1, (size_t)n, f) : 0;
    fclose(f);
    return (w == (size_t)(n > 0 ? n : 0)) ? 1 : 0;
}

int64_t ctron_fio_read(const char* path, ctron_view_w8u buf) {
    FILE* f = fopen(path, "rb");
    if (!f) { return -1; }
    int64_t r = (int64_t)fread(buf.d, 1, (size_t)buf.n, f);
    fclose(f);
    return r;
}

// 读整个文件,跳过首行(定位首个 0x0A),余量拷入 buf —— CAS 载体
// "地址行\n原始载荷" 的读原语(loom 传承语义;首行后全量,视图容量截断)
int64_t ctron_fio_read_after_line(const char* path, ctron_view_w8u buf) {
    FILE* f = fopen(path, "rb");
    if (!f) { return -1; }
    size_t cap = 1 << 18;
    uint8_t* tmp = (uint8_t*)malloc(cap);
    if (!tmp) { fclose(f); return -3; }
    size_t r = fread(tmp, 1, cap, f);
    fclose(f);
    size_t i = 0;
    while (i < r && tmp[i] != 10) { i++; }
    if (i >= r) { free(tmp); return -2; }
    size_t n = r - i - 1;
    if (n > (size_t)buf.n) { n = (size_t)buf.n; }
    if (n > 0) { memcpy(buf.d, tmp + i + 1, n); }
    free(tmp);
    return (int64_t)n;
}

// Str → 原始字节(按显式长度;多字节保真)
int64_t ctron_fio_str_bytes(const char* s, int64_t n, ctron_view_w8u out) {
    if (n > out.n) { return -1; }
    if (n > 0) { memcpy(out.d, s, (size_t)n); }
    return n;
}

// 排他创建写(0600;密钥文件纪律):1 = 新建成功;2 = 已存在(不覆盖);0 = 失败。
// data = GC NUL 结尾串,按显式 n 写(密钥面恒 ASCII hex + \n,无内嵌 NUL)
int32_t ctron_fio_write_excl_0600(const char* path, const char* data, int64_t n) {
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) { return (errno == EEXIST) ? 2 : 0; }
    ssize_t w = n > 0 ? write(fd, data, (size_t)n) : 0;
    close(fd);
    return (w == n) ? 1 : 0;
}

// 文件字节数(-1 = 不存在/失败;增量索引的 size 变更检测原语)
int64_t ctron_fio_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) { return -1; }
    return (int64_t)st.st_size;
}

// 偏移读(pread;返回字节数,0 = 文件末尾,-1 失败——段式追加存储的尾随原语)
int64_t ctron_fio_read_at(const char* path, int64_t offset, ctron_view_w8u buf) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { return -1; }
    ssize_t r = pread(fd, buf.d, (size_t)buf.n, (off_t)offset);
    close(fd);
    if (r < 0) { return -1; }
    return (int64_t)r;
}

// 当前工作目录绝对路径(字节数;-1 = 失败/容量不足——getcwd 对过小缓冲返 NULL 不截断)
int64_t ctron_fio_getcwd(ctron_view_w8u buf) {
    if (buf.d == NULL || buf.n <= 0) { return -1; }
    if (getcwd((char*)buf.d, (size_t)buf.n) == NULL) { return -1; }
    return (int64_t)strlen((char*)buf.d);
}
