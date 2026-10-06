/* minilibc dlfcn.h —— 动态链接族仅声明(bare 无 dlopen,§9.3 零 OS 依赖;
 * 发射运行时 #[dlsym] thunk 块为 static 死代码的编译面)。 */
#ifndef CT_MLC_DLFCN_H
#define CT_MLC_DLFCN_H

#define RTLD_DEFAULT ((void*)0)
#define RTLD_NEXT ((void*)-1)
#define RTLD_LAZY 1
#define RTLD_NOW 2

void* dlopen(const char* path, int mode);
void* dlsym(void* h, const char* sym);
int dlclose(void* h);
char* dlerror(void);

#endif
