/*
 * ffi c_src 垫片 —— strerror 直声明不可行之故:
 * Ctron 的 Str 边界映射 const char*,libc strerror 返回 char*(string.h
 * 原型在发射产物 include 集内),const 限定符冲突。经自有符号中转,
 * 消费方链接本文件(net、tls 同款机制)。
 */
#include <string.h>
#include <stdint.h>

const char* ctron_ffi_strerror(int32_t e) {
    return strerror((int)e);
}
