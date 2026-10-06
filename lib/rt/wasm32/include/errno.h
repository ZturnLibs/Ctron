/* minilibc errno.h —— 定义驻 bare_libc.c;发射运行时引用 errno 的块均为
 * static 死代码(DCE),此存根仅为编译面。 */
#ifndef CT_MLC_ERRNO_H
#define CT_MLC_ERRNO_H
extern int errno;
#endif
#define EEXIST 17
