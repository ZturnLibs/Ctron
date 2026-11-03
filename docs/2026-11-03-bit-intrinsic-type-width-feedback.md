# 反哺报告:bit.* 内建静态型别缺席 → 循环节 I64 位算发射 I32 降型假 panic

- 日期:2026-11-03
- 提报:Loom 执行会话(检查算术新面消账时发现)
- 基线:Ctron main `1fee8724`(含检查算术默认开)
- 定性:**编译器缺陷(发射面静态型别表缺席)**,非 Loom 逻辑错、非有意回绕

## 症状

Loom 全量门 `check_blob`(131072B blob 的 CDC 滚动分块哈希)发射件即崩:
`integer overflow (+)`。源码全程 I64(滚动哈希协议常量掩码 0xFFFFFFFF):

```
var h: I64 = 0
while i < n {
    let g = bit.and_i64((b + 1) * 2654435761, 4294967295)
    h = bit.and_i64(bit.shl_i64(h, 1) + g, 4294967295)
    ...
}
```

## 最小复现(双通道)

`/tmp/rD.ct`:while 循环体含 `let g = bit.and_i64((b + 1) * 2654435761, 4294967295)` +
`h = bit.and_i64(bit.shl_i64(h, 1) + g, 4294967295)`。

- interp(`ctron-cc run`):值正确(掩码模 2^32 语义);
- emit→cc(`ctron-emit run` → cc):大值路径 panic。发射 C 实证(loom build/loom.c:17463 同形):

```c
/* 错(循环节内):I64 运算被降型 I32 检查 */
t_h = (int64_t)(((uint64_t)(ctron_w_add((int32_t)(ct_bit_shl_i64((int64_t)(t_h), (int32_t)(1))),
      (int32_t)(t_g), (-2147483647 - 1), 2147483647))) & ((uint64_t)(4294967295LL)));
/* 对(直排语句):i64_add 形,LHS 推断幸免 */
t_h = (int64_t)(((uint64_t)(ctron_i64_add((int64_t)(ct_bit_shl_i64(...)), (int64_t)(t_g)))) & ((uint64_t)(4294967295LL)));
```

判定关键:掩码语义下 I32 截断本不改变模 2^32 结果,但 w_add 以 **I32 有符号界**
检查截断后操作数,h/g∈[0,2³²) 时截断值异号相加越 I32 界 → 假 panic
(统计上大文件数百迭代内必触)。drv_parity:同一源 interp 过 / emit 崩。

## 根因定位

`compiler/src/trans_ty.ct` `ct_typeof`(:1263):**无 bit.\* 内建分支**——位内建调用
型别恒落默认 `"i"`(I32)。循环节内 `h = bit.and_i64(<expr>, …)` 赋值按 RHS 型别
"i" 选 w_add I32 臂;直排语句因 LHS 变量环境型("6")走 I64 臂。语义层
(eval_call.ct `ns_bit`/`bit_ret64` = v6i 即 I64 值标签)解释臂自洽——纯发射面
静态型别缺席。

## 修复(已实施,待门)

trans_ty.ct `ct_typeof` Call-Member 臂增 bit.* 分支:按名后缀定宽,
`*_i64 → "6"`、`*_u64 → "7"`(uint64_t),_i32/_u32 维持默认 i——与 ns_bit
运行期宽度解析同口径。Loom 源零改动(无屏蔽性特判)。

## 附带发现(未修,登记)

interp 常量折叠形:`h = bit.and_i64(bit.shl_i64(h,1) + g, mask)` 直排字面初值 +
`if h == <literal>` 比较形,interp 判真与逐值打印形(值 2147483647,即正确掩码
结果)相抵——同一表达式两语句形结果不一,疑 check 期折叠路径宽度差。独立
低优先项,待专窗。

## 第二发现:While 臂修复(1fee8724)半截迁移 → 自举必死(已回退处置)

`1fee8724` 于 trans_stmt.ct 四处将自包含 drop 哨兵(While 入口
`ctron_drop_mark()` + 首降册位惰性压哨 + 出口/break/continue
`ctron_drop_unwind_mark(0/1)`,b954ef49 形态)迁移为静态标记变量
`int32_t t_dm<nline(条件节点)> = ct_drn` + Break/Continue 臂
`ctron_drop_unwind_to(ct_env_ty(env,"#whdmark"))`,但:

1. **"#whdmark" 写侧(env_bind)未落地**;`ct_env_ty` 未命中回退 `"i"`
   (I32 型别码)被直发为 C 标识符 → `ctron_drop_unwind_to(i);` 未声明
   ×20(compiler 全部环内 break 位)→ **native.sh 自举必死**;
2. 条件表达式节点部分形态无 nstamp 尾槽,`nline` 回落 `"0"` → 多环同名
   `t_dm0` 重定义(1fee8724 提交件 bin 非本源所建:src 12:13/bin 12:33);
3. `whd` 哨兵标志恒 false 死代码。

**处置 = 回退至 b954ef49 自包含哨兵形**(四处 hunk 逆向;1fee8724 真内容
byte_at 行源修复与 While 条件行源槽保留不动),双镜像纪律无解析面变更。
bit 型别修复(trans_ty.ct ct_typeof bit.* 分支)独立保留。
