/* minilibc stdio.h —— bare 档 stub 头(T40 §9.3)。
 * 面向 ctron 发射 C 的编译面:stdout/stderr 系 semihosting;FILE 族仅声明
 * (发射运行时 fs 块为 static,未被 bare 程序引用时被 --gc-sections 裁除)。 */
#ifndef CT_MLC_STDIO_H
#define CT_MLC_STDIO_H

#include <stddef.h>

struct _ct_file { int fd; };
typedef struct _ct_file FILE;
extern FILE* stdout;
extern FILE* stderr;
extern FILE* stdin;

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int printf(const char* fmt, ...);
int puts(const char* s);
int putchar(int c);
int fflush(FILE* f);
int fprintf(FILE* f, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t n, const char* fmt, ...);
int vsnprintf(char* buf, size_t n, const char* fmt, __builtin_va_list ap);

FILE* fopen(const char* path, const char* mode);
int fclose(FILE* f);
size_t fread(void* p, size_t sz, size_t n, FILE* f);
size_t fwrite(const void* p, size_t sz, size_t n, FILE* f);
int fseek(FILE* f, long off, int whence);
long ftell(FILE* f);
int remove(const char* path);

#endif
