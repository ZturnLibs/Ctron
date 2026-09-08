// trans.h —— C10-a:Ctron → C 转译后端(数值域骨架)。
// v1 范围:整宽全集/浮点/Bool、检查算术(溢出/除零 panic 消息逐字对齐 rt.c)、
//          if/while/for-in-range、函数定义与递归、test 块 + assert/assert_eq、print/println、
//          panic(字面量)、const 之外声明族。
// 明确拒绝(v1 外):Str/数组/切片/struct/class/enum/match/闭包/Option/Result/?/GC/own/scope。
#ifndef CTRON_TRANS_H
#define CTRON_TRANS_H

#include "arena.h"
#include "ast.h"

typedef struct {
    char* code; // 生成的 C 源(malloc;调用方 free)
    char* err;  // 非空 = 不支持的构造(malloc;NULL = 成功)
} ctron_trans_result;

ctron_trans_result ctron_trans_file(const cfile* f);
void ctron_trans_result_free(ctron_trans_result* r);

#endif
