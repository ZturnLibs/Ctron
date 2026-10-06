/* bare 桩族:sys/stat.h(FB-2 fs_mkdir 发射面内联 include 的编译承接;
 * bare 无文件系统——mkdir 恒败,实现在 bare_libc.c,--gc-sections 随未引用裁剪) */
#ifndef CTRON_BARE_SYS_STAT_H
#define CTRON_BARE_SYS_STAT_H
typedef unsigned int mode_t;
int mkdir(const char* path, mode_t mode);
#endif
