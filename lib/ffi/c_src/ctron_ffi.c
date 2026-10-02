/*
 * ffi c_src 垫片 —— strerror 直声明不可行之故:
 * Ctron 的 Str 边界映射 const char*,libc strerror 返回 char*(string.h
 * 原型在发射产物 include 集内),const 限定符冲突。经自有符号中转,
 * 消费方链接本文件(net、tls 同款机制)。
 *
 * ctron_cbox_* —— CBox[T] 所有权登记表(T45 §9.6 约定之 2):Ctron 侧
 * 泛型 extern 单态化为声明态码(int32_t*),此处以 void* 承接——指针
 * 调用约定同型,跨 TU 原型差异不涉 ABI。登记表即所有权哨(注册/注销),
 * struct 值拷贝不复制哨状态;仅许接 malloc 族块。
 */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

const char* ctron_ffi_strerror(int32_t e) {
    return strerror((int)e);
}

#define CTRON_CBOX_CAP 256
static void* ctron_cbox_tab[CTRON_CBOX_CAP];
static int ctron_cbox_n = 0;

void ctron_cbox_reg(void* p) {
    if (ctron_cbox_n < CTRON_CBOX_CAP) {
        ctron_cbox_tab[ctron_cbox_n++] = p;
    }
}

int ctron_cbox_live(void* p) {
    int i;
    for (i = 0; i < ctron_cbox_n; i++) {
        if (ctron_cbox_tab[i] == p) return 1;
    }
    return 0;
}

void ctron_cbox_take(void* p) {
    int i;
    for (i = 0; i < ctron_cbox_n; i++) {
        if (ctron_cbox_tab[i] == p) {
            ctron_cbox_tab[i] = ctron_cbox_tab[--ctron_cbox_n];
            return;
        }
    }
}

void ctron_cbox_drop(void* p) {
    ctron_cbox_take(p);
    free(p);
}
