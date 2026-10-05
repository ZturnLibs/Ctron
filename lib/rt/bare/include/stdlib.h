/* minilibc stdlib.h —— 堆族仅声明(发射运行时 GC 块未被 bare 程序引用时裁除);
 * exit/abort 真实现走 semihosting(SYS_EXIT)。 */
#ifndef CT_MLC_STDLIB_H
#define CT_MLC_STDLIB_H

#include <stddef.h>

void* malloc(size_t n);
void* calloc(size_t a, size_t b);
void* realloc(void* p, size_t n);
void free(void* p);

void exit(int code) __attribute__((noreturn));
void _exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

char* getenv(const char* name);
int abs(int v);
long labs(long v);
long long llabs(long long v);

/* 转换族:仅声明(发射运行时 GC env/extern thunk 死代码编译面;DCE 后无引用) */
long atol(const char* s);
long strtol(const char* s, char** endp, int base);
double strtod(const char* s, char** endp);
float strtof(const char* s, char** endp);

#endif
