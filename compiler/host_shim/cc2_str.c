/* cc2_str.c —— CC2(2026-10-16):str_byte/utf8_enc 解释臂宿主口
 *
 * 背景:loom 字节平面读出(kx_get/cas_get hex 解码)需要"单字节保真"的
 * byte→Str 构造;原唯一通道 utf8_enc 是码点→UTF-8 编码器,逐字节用于
 * ≥0x80 即双重编码(非 ASCII 全线乱码)。且解释臂 utf8_enc 原实现经
 * dvi_str 递归自调恒返 U+FFFD(发射臂正常)——同批修复。
 *
 * 链接:随 native.sh 编入 ctron-cc(及含 eval 的驱动);发射臂用户程序
 * 的 str_byte 走 driver_emit 样板 ctron_str_byte(样板内 GC)。
 *
 * 内存约定:本 TU 无法触达样板 static ct_gc_alloc,故用 malloc——
 * 解释器 Val.s 别义持有,GC 不拥有故不释放(解释器=开发档,每次调用
 * 有界 ≤5B 泄漏,自检量级毫秒;产品走发射臂无此路径)。
 * 参数协议:dec = 字节/码点的十进制文本(对齐 ctron_fs_mkdir_p 直调桥形)。
 */
#include <stdlib.h>

const char* ctron_str_byte_t(const char* dec) {
    char* r = (char*)malloc(2);
    if (!r) { abort(); }
    r[0] = (char)(strtol(dec, 0, 10) & 0xFF);
    r[1] = 0;
    return r;
}

const char* ctron_utf8_enc_t(const char* dec) {
    int cp = (int)strtol(dec, 0, 10);
    if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { cp = 0xFFFD; }
    char* r = (char*)malloc(5);
    if (!r) { abort(); }
    int un = 0;
    if (cp < 0x80) {
        r[un++] = (char)cp;
    } else if (cp < 0x800) {
        r[un++] = (char)(0xC0 | (cp >> 6));
        r[un++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        r[un++] = (char)(0xE0 | (cp >> 12));
        r[un++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        r[un++] = (char)(0x80 | (cp & 0x3F));
    } else {
        r[un++] = (char)(0xF0 | (cp >> 18));
        r[un++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        r[un++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        r[un++] = (char)(0x80 | (cp & 0x3F));
    }
    r[un] = 0;
    return r;
}
