# eval 值/流表示换代(Val struct)——设计 v1(立项待批)

> 状态:**待批**(设计先行;实现未动工)
> 作者:编译器线(2026-09-26 会话);上游:eval-repr 计划(2026-09-25-eval-repr-overhaul.md,
> 其 c6 快路径与字面量直累加已落库 487eaef,native 臂 20×)
> 一句话:解释器的值/环境/流从「字符串编码 List」换代为「标签结构体」,
> 标量算术归零 parse/format 与装箱分配,解释口径整体再取 5-10×。

## 0. 为什么(用户价值)

解释器 = 语言面向用户的脚本执行态(compile 才是性能态)。当前每次标量值诞生
= 表头+push×2+to_string ≈ 4-5 次 arena 分配,每次比较可能 parse/format 往返;
实测 loop 基准 810 次/迭代、crypto 自测 7.2 亿次/1.5GB/86s。换代后标量路径
字段直取:解释口径预计再 5-10×,crypto 回归分钟内、豁免可回收,用户脚本
直享同倍解释提速。

## 1. 现役表示(v1 实读)

- **值** = `List[Str]`:[0]=类别 tag("I"/"W"/"6"/"7"/"B"/"S"/"D"/"V"/"T"/"U"),
  [1..]=文本载荷。构造器 vI/vB/vS/v6/vV2 各 4-5 次分配;算术 = 文本 parse
  →运算→to_string(dvi/txt_num/c6 族);vcmp 跨宽比较含 dvi().to_string() 往返。
- **env** = 平铺 `List[Str]`,条目 = [名,值] 二元子列表;bind/set 全量拷贝。
- **流** = e4/s4 元组(kind/env/val/out 或 kind/env/out/val),每表达式节点一个。
- **分配画像**(loop 基准,c6 快路径后):810 次/迭代;32B/64B 桶成对
  (列表头+items 数组)= e4 元组与值列表;16B = 小串/格。

## 2. 目标表示(V2)

```ctr
struct Val {
    var tag: I32      // 0 void/1 I32/2 I64/3 U64/4 Bool/5 Str/6 F64/7 struct/enum/8 复合句柄
    var iv: I64       // 整数族原生槽(I32/I64/U64≤2^63-1)
    var b: Bool       // Bool 槽
    var s: Str        // Str/复合句柄槽(复杂值沿用现有串编码,零迁移)
}
struct Entry { var nm: Str
    var v: Val }
struct Flow { var kind: I32
    var env: List[Entry]
    var v: Val
    var out: Str }
```

- **标量端到端原生**:vI(x) = Val{1, x} 零 arena 分配(值语义结构体,编译为
  C 结构体直传);算术 = 字段运算;比较 = 字段比较;to_string 仅在 println/
  插值出口发生(本就必须输出)。
- **复杂值零迁移**:struct/enum/闭包/通道/Box 沿用现有串编码入 Val.s
  (现役表示本就是串编码,位面不变)。
- **env/流随行**:Entry/Flow 结构体顺带消 e4 元组与二元子列表;
  帧化(Phase 1 设计)作为 env 结构的一部分同期落(Entry 栈天然分帧)。

## 3. 分期

- **S0 尖峰(半日)**:Val struct + vI/v6/vcmp 三点最小改,loop 探针电池 +
  AMEM 对账——验证「结构体值零分配」在自举两侧(seed 解释/native)都成立,
  并暴露 struct 在 cc_run 拼装里的任何自举坑(如 seed 对编译器自用 struct 的
  支持)。
- **S1 标量迁移(2-3 日)**:vI/v6/vB + w_arith/c6 快路径改字段运算 +
  env/Flow 换 Entry/Flow 结构。门:suite 自举 73/73、neg/arith 探针电池、
  smoke 与基线等价、AMEM loop ≤ 200/迭代。
- **S2 复合值收尾(1-2 日)**:T/U/D/CH/BOX 面逐 tag 迁移或显式串编码留存;
  fmt/eq/eval_str 出口适配。门:全 std 模块自举臂(含 crypto)、parity。
- **S3 豁免回收(半日)**:crypto 回 3j2/4c;bench 刻度复校。

## 4. 风险与边界

- **自举自用 struct 的工具链成熟度**:S0 尖峰先行验证(seed 解释编译器自源
  struct、两口径 emit、AMEM 对账),不通则本立项降级为「仅 c6cmp/dvi 级
  微修 + 维持现状」,已预留。
- **语义等宽陷阱**:I32 折回(as[I32] 截断)、U64 跨 2^63 比较序、"-0"
  符号感知序(现役 quirky 行为承载于 time.ct,换代须逐项对拍)——探针电池
  固化全部已知 quirky 点后再动刀。
- **回退**:每期单提交;Val 未全量切换前新旧表示以适配器共存,S1 不达门即整期回退。
- **边界**:不动 emit 编译路径;不动 seed;不改 Ctron 语言面。

## 5. 验收汇总

| 门 | 现值 | 目标 |
|---|---|---|
| loop 解释分配/迭代 | 810 | ≤ 200(≥4×) |
| crypto 自测 | 86s/7.2 亿次/1.5GB | ≤ 30s/≤ 3 亿次,回收豁免 |
| suite 自举 | 73/73 | 73/73 |
| smoke | 基线等价 | 基线等价 |
| 用户解释口径 | 基线 | ≥ 3×(抽 scenes 脚本三件计时) |
