/* minilibc dirent.h —— 目录族仅声明(fs 层 std 档级;发射运行时 ctron_read_dir
 * 为 static 死代码,编译面存根)。 */
#ifndef CT_MLC_DIRENT_H
#define CT_MLC_DIRENT_H

typedef struct _ct_DIR DIR;
struct dirent { long d_ino; unsigned char d_type; char d_name[256]; };

DIR* opendir(const char* path);
struct dirent* readdir(DIR* d);
int closedir(DIR* d);

#endif
