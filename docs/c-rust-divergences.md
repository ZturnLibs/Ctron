# C 版与 Rust 版语义/能力分歧清单

> 来源:2026-09-07 性能对比(见 `tools/bench.py`)过程中的实测发现。
> 口径:双实现共享 `docs/spec` 规范与 `tests/` 语料;分歧分两类——**待修项**(某版
> 违反规范或存在解析缺陷,须对齐)与**行为差异**(能力面/上限不同,记录在案,是否
> 收敛由规格所有者裁决)。

## 待修项

### T1. 解析器:`x[expr] {` 被误判为泛型实参(双解析器同款)——已修复

`if` 条件等位置的**下标访问紧跟块起始**时,`]`+`{` 前瞻启发式(`bracket_followed_
by_call_or_lit`)把它当成 TypeArgs+结构字面量,产生 E1001:

```ct
if a[j] > a[j + 1] { ... }   // 修复前:C、Rust 解析器均 E1001
```

**勘误**:初版档案记"Rust 版正常"系误导——Rust `run` 对解析错误静默 exit 0
(见 D5),早期探针据此误判。实为双解析器同款缺陷。

**修复(2026-09-07)**:双版 `bracket_followed_by_call_or_lit` 收紧为仅 `](` 触发
(`List[I32](…)`/`Atomic[I32](0)` 等构造调用);`]/{` 不再触发——`Pair[T] { }`
形态双版语法皆不存在,`x[expr] {` 只能是下标后跟块。回归锚 `tests/01g_index_
block.ct`(双版 check 零诊断/run 一致/ast_diff 逐字节一致;campaign 52/52)。

### T2. `+` 字符串拼接:C 执行层接受,双版 check 层拒绝

```ct
var s = ""
s = s + "x"        // C check:E2010 拒;C rt/trans:接受;Rust rt:panic "算术需要数值"
```

**层面对齐矩阵(2026-09-07 实测)**:拒绝阵营 = C check + Rust rt;接受阵营 = C rt +
C trans。C 版三层不自洽(check 拒、执行层收)。

**成本证据(关键)**:`selfhosted/*.ct`(ev_num/cc 等树打印代码)大量使用
`out = out + ", "` 形态,跑在 C 种子解释器上——收紧 C rt 将打断自举固定点,
需千级位点改写为插值。

修法三选一,待规格所有者裁决:
(a) C 收紧(对齐规格)= 自举线大改写,**代价最高,不建议**;
(b) 规格修订接纳 `+` 拼接 → Rust rt/trans 各补一处 binop 分支,C check 同步放开
    E2010 的 Str 分支(修复三层不自洽)——**证据倾向此项**;
(c) 维持现状,文档化为「check=规格严格档,执行层=宽容档」,接受双面性。

**状态:规格已裁决方向 (b),2026-09-08 落地**(规格 §4.5 修订 + C sem 放行 +
Rust interp/check/trans 对齐;回归锚 `tests/01h_str_plus.ct` 四路逐字一致)。
**已收敛(2026-09-08)**:T2 落地后追查,连修四个 Rust interp 语义缺陷——
① `&&` 不短路(LHS 假仍求值 RHS);② UInt/Int 混合算术兜底拒绝(补 i128 中介
分支);③ Str `[]` 下标静默返回 Void(收紧为 panic「索引目标非数组」,对齐 C rt);
④ `.len`/`char_len` 返回 UInt(对齐 C v_int 有符号;byte_at/byte_slice/read_bytes
参数同步强化)。另补 `List[T]()` 构造器与标量 `to_string`。
**里程碑:cc.ct 全管道双种子逐字一致**——input_small/pay(双版同点 abort)+
input_cc/cc2/cc3(21/8/11 行输出 + 退出码),五个输入 diff 全零。
范畴:本项为「自举代码 × Rust interp 域验收」首批成果;后续随自举泳道
cc.ct 演进持续跑 `genmod` 矩阵即可。

## 行为差异(记录在案)

### D1. CLI 运行入口 —— 已对齐(2026-09-08)

Rust 版补齐:`interp::run_main`(const/static 预求值 + main 体执行,EarlyReturn
→ 退出码)+ lib `run_main_file`(解析诊断非空 → Err)+ CLI `run` main 优先分派
(有 fn main 跑 main,否则 test 块;退出码 = main 返回值)。
随附解释器内建补齐:println/print(fmt_val 同格式)、read_file(Option[Str])/
read_line/read_bytes/flush_out、byte_at/byte_slice、`List[T]()` 构造器。
残余:`cc.ct` 全管道在 Rust interp 上仍被 T2 阻塞(见上)。

### D5. Rust `run`/`test` 对解析错误静默成功

文件存在解析诊断时,Rust `run` 不报错、无输出、exit 0(其 `run_test_file` 内部
解析失败 → 测试表为空 → 失败计数 0);C `run` 打印诊断并 exit 1。曾致 T1 的
"Rust 版正确"误判——排查 Rust 侧行为时务必用 `ctron check` 而非 `run` 验证解析。

### D2. 解释器步数上限:机制已对齐(CTRON_MAX_STEPS),默认值有意保留差异

双版解释器支持同名环境变量 **`CTRON_MAX_STEPS`**(N = 步上限,超限 panic
"instruction limit exceeded (可能的无限循环)";`0` = 无限;未设/非法 = 各自默认):

| 实现 | 默认 | 理由 |
|---|---|---|
| Rust interp | 2_000_000 | 防无限循环护栏(原行为保留) |
| C rt | 无限(0) | 自举负载(cc.ct 解释自身)远超 2M 步,护栏默认会打断阶梯 |

默认值统一待规格所有者在工具链章节(B 口径)定案;在此之前以本档为准。
2026-09-07 验证:300K 迭代负载 Rust 默认触发/`0` 档解锁,C 全档通过。

### D3. 转译后端形态支持面

- `fn main` 文件:双版均完整发射(2026-09-08 Rust 补齐:无 test 且有 main 时
  发射 `int main(void) { alarm(20); return (int)ctn_main(); }`;main 体本就随
  用户 fn 发射为 ctn_main)。
- `println`/`print` 发射:双版均支持(2026-09-08 Rust 补齐 ct_print_* 家族,
  格式面 = 解释器 fmt_val:浮点整值 %.1f 否则 %g)。
- `09_simd`:C 后端已覆盖(34/34);Rust 后端未覆盖(33/34)。

### D4. 检查器档案

C 版 sem 的 E2010/E3040 等为 Rust `check.rs` 的保守子集(推导不出不报),检查面
更小——性能对比中 check 阶段 C 快 2.6× 有一部分源于此,不能全记为实现优势。

## 服务器面(2026-09-20)

服务器泳道(S0+P1,`tests/net/` 全绿)执行中实测的跨面事实登记;细节见
`docs/superpowers/plans/2026-09-20-server-s0-p1.md` 执行记录(Task 4–9)。

### (a) FFI 边界事实(Task 4 探针 + Task 5–7 垫片/ctecho 实证)

- **可用签名白名单**:标量 / `Str` / `&I64[]`·`&I32[]`(视图复合字面量,零拷贝)/ `Box[struct]`(按值堆胞指针,`b->v` 与 `b.v` 互为镜像)。垫片与用户 extern 均按此选型。
- **裸 `T[N]` extern 形参静默发射 `int32_t`**(ct_ctype 无 `a<N><码>` 映射)——不可作边界签名,且无诊断,静默错型。
- **未初始化 `var buf: T[N]` 致 ctron-emit 段错误(exit 139)**——编译器缺陷,夹具一律带初始化器规避(转编译泳道在册)。
- **`&Struct` 形参发射 `t_&`(非法 C)**——struct 按引用出参只走 `Box[struct]`。
- **`Result[Struct, _]` 的 Ok 成员访问发射即 panic**;**trait 方法调用不发射**;**class 字面量不可发射** → 落 struct。
- **spawn 仅标量过界且须有返回值**;**while 体内禁 Drop 局部**(emission 面限制,垫片以显式 close 规避)。

### (b) C 宿主 caps 键集已收账(M-T3-1)

`compiler-c/src/pkg.c` check_caps 的 E4010 键集原硬编码 `{fs,time}`,与自举侧
`compiler/src/parse_pkg.ct`(Task 3 已放宽为 `{fs,time,net,db}`)不一致——
宿主对 net/db 触网包静默放行(caps_net 宿主侧红)。2026-09-20 同口径放宽,
modules 段宿主pkg 8/10 → 9/10,caps_net 双线绿;R 线 `check.rs` 的
`caps_used` 本就按 `cap_key_by_def` 通用收集(无硬编码),三线键集口径自此一致。

### (c) ctecho 解释口径不支持

`examples/ctecho`(及 tests/net 全套)走 emit-only 验收(ctron-emit → cc →
原生运行);解释器 W4 桥未含 net 垫片,`use std.net.*` 在解释口径无绑定——
这不是缺陷而是口径边界:网络面在解释器内的支持未排期,夜间/CI 均走原生路径。

### (d) R 线 caps 泛化与 r7b 翻转状态

R 线 E4020(pure 触网)的 caps_used 收集已泛化(见 (b));`tests/roadmap/
r7b_pure_net.neg.ct` 今天为 **RunRed**——跨函数纯度传播未实现(`#[pure]`
经间接调用洗白能力调用不产 E4020),翻转待 R 线 std/net 解析就绪,按翻转
协议迁 NegGreen(E4020)。另:UDP 真回环收发(端口协调面)归 nightly,
本地门禁只钉调用面与 500 轮 fd_churn。

---
维护约定:新发现分歧先记本档(附最小复现),修复后在条目标注 commit。
