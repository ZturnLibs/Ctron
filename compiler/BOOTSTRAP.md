# BOOTSTRAP —— Ctron 自举方案(参考文档)

> 本文记录 compiler/ 自举链的完整方案:链条结构、关键机制、三级证明的复现命令、
> 验收面,以及实现过程中踩到的不变量与坑。供后续扩展发射器、换后端、或在新机器
> 上重建自举时参考。用法速查见 README.md;本文讲"为什么这样设计"与"怎么复现"。

## 1. 自举链总览

```
G0  C 宿主 seed(compiler-c/build/ctronc)          —— 唯一的宿主依赖,仅首次引导
     │  ctronc run cc_emit.ct(经 ANCHORINPUT 换靶,输入 = cc_emit 源自身)
     ▼
G1  原生发射器 bin/ctron-emit(12366 行 C)        —— Ctron 写的编译器的产物
     │  ctron-emit run cc_emit.ct(CLI 覆锚,无需再换靶)
     ▼
G2  原生发射器(自编译产物)
     │  ctron-emit-g2 run cc_run.ct
     ▼
G2' 原生编译器 bin/ctron-cc(运行驱动:parse → 语义 12 项 → 解释执行)
```

要点:

- **首次引导后链自持**:G1 起不再需要 C 宿主——每一代发射器都能编译出
  发射器自身(下一代的自己)与运行驱动编译器,产物逐字节稳定。
- 源头只有一份 Ctron 源:`src/` 六模块;`build.sh` 确定性拼接出三个单文件产物。
- "Ctron 写的编译器"同时以两种形态存在:被 seed **解释**执行(自译化面),
  和被发射器**编译**成原生二进制(自举面)。两条路互为印证。

## 2. 组成与角色

| 件 | 角色 |
|---|---|
| `src/lex.ct` `parse.ct` `sem.ct` `eval.ct` | 编译器核心四模块(167 fn),三产物共享 |
| `src/trans.ct` | C 代码生成器 v3(30 fn):`t_` 用户符号 / `ctron_*` 运行时 / 类型码 i·s·b·f·L·A·N |
| `src/driver_run.ct` | 运行驱动入口:parse → 语义 12 项 → 解释执行 main/test |
| `src/driver_check.ct` | 检查驱动入口:parse → 语义 12 项即止,打印 `check OK decls=N` |
| `src/driver_emit.ct` | 发射驱动入口:parse → 生成等价 C(产物带 `main(argc,argv)` CLI) |
| `build.sh` | 确定性拼接:`cc_run/cc_check = 四核心+各自驱动`;`cc_emit = 四核心+trans+发射驱动` |
| `ctc.sh` | 用户入口:`ctc.sh <in>` 运行 / `ctc.sh check <in>` / `ctc.sh emit <in> [out.c]` |
| `native.sh` | 自举编译:发射器编译 `cc_run.ct`/`cc_emit.ct` → C → 本机 cc → `bin/` |
| `test/smoke.sh` | 自举验收 18 项(快面 15 + `--full` 自发射收官与固定点) |
| `test/suite.py` | tests/ 一致性套件跑分(可 `CTRON_CC=<bin>` 覆盖被测编译器) |

宿主 seed 的**唯一职责**是把 `cc_emit.ct` 变成第一个原生发射器(native.sh 内部的
`ctc.sh emit`,做一次 ANCHORINPUT 换靶)。此后一切编译动作由 Ctron 编译器自己完成。

## 3. 关键机制

### 3.1 单文件程序模型 → 确定性拼接

Ctron 当前无本地多文件模块,模块化以 `cat` 拼接实现。顶层 decl 只有无副作用的
`fn`/`Static`/`Const`,且宿主对顶层 fn 顺序不敏感(调用按名解析),因此拼接顺序
只影响可读性。核心 167 fn 三产物共享,修一处三产物同时生效。

### 3.2 read_file 锚与两级换靶

驱动 main 里有一处字面量锚,是编译器"读哪个输入"的唯一通道:

| 驱动 | 锚字面量 | 换靶方式 |
|---|---|---|
| driver_run / driver_check | `read_file("../selfhosted/input_cc.ct")` | `sed "s|\.\./selfhosted/input_cc\.ct|$IN|"` |
| driver_emit | `read_file("ANCHORINPUT")` | `sed "s|ANCHORINPUT|$IN|"` |

**第二级换靶在产物里**:发射产物带 `main(argc,argv)`,`<bin> run <file>` 置
`ctron_cli_input`,其后一切 `read_file` 一律返回该文件(C9j⑥)。因此原生发射器
编译任意 .ct 无需再 sed——`ctron-emit run x.ct > x.c` 即是。两级换靶是
"seed 需要换靶、原生不需要"差异的来源。

### 3.3 两个驱动、一类核心

run 与 emit 共享 parse+sem+eval 核心,只换最后的 main。发射驱动**不**跑语义门
(语义 12 项是 run/check 路径的守门);需要"检查后再发射"时由脚本串联
`ctc.sh check && ctc.sh emit`。

### 3.4 发射产物形态

- 运行时辅助一律 `ctron_*` 前缀(arena 分配 ctron_amalloc、List/Cell/字节域/
  read_file/read_dir/panic),用户符号 `t_` 前缀,函数原型前置(前向引用/递归)。
- 产物自带 CLI:`if (argc >= 3 && strcmp(argv[1], "run") == 0) ctron_cli_input = argv[2];`
- 产物即通用二进制:发射驱动产物 = 编译器;运行驱动产物 = 解释器。

## 4. 自举的三级证明(均脚本化,可复现)

### L1 解释层自译化 —— 编译器解释编译器解释用户程序

```bash
selfhosted/cc.sh <(无参)          # 或完整阶梯:
cd selfhosted && ./ladder.sh --full   # 第 5 步,~138s,输出与黄金逐字一致
```

### L2 编译层自举 —— 编译器源 → 原生二进制可用

```bash
compiler/native.sh                # ~9s:发射器编译 cc_run/cc_emit → gcc → bin/
compiler/test/smoke.sh --full     # 18 项:黄金/自编译/发射往返/自发射/固定点
```

### L3 跨代固定点 —— G1 编译出 G2,G2 再编译逐字节相同且完全可用

```bash
T=/tmp/selfgen && rm -rf $T && mkdir -p $T && cd <repo根>
# G2 发射器 = G1 编译发射器源自身
compiler/bin/ctron-emit run compiler/build/cc_emit.ct > $T/g2_emit.c
cc -O2 -w -o $T/ctron-emit-g2 $T/g2_emit.c
# G2 编译器 = G2 编译编译器源
$T/ctron-emit-g2 run compiler/build/cc_run.ct > $T/g2_cc.c
cc -O2 -w -o $T/ctron-cc-g2 $T/g2_cc.c
# 验证:黄金 + 负例 + 跨代逐字节
$T/ctron-cc-g2 run selfhosted/input_cc3.ct | diff - selfhosted/expected/input_cc3.out
compiler/bin/ctron-emit run compiler/build/cc_emit.ct | diff - $T/g2_emit.c   # 逐字节
CTRON_CC=$T/ctron-cc-g2 python3 compiler/test/suite.py                        # 50/50
```

2026-09-08 实测:G2 编译器黄金逐字一致 + 负例 rc=1 + 套件 50/50 + 两代发射产物
逐字节相同。**固定点跨代成立 = 自举链已收敛,后续重编译不会漂移。**

## 5. 验收面(三线互证)

| 面 | 命令 | 通过标准 |
|---|---|---|
| 自举验收 | `compiler/test/smoke.sh --full` | 18 ok / 0 fail |
| 语言一致性 | `python3 compiler/test/suite.py` | 50/50(与 C 宿主对齐) |
| 旧快照阶梯 | `cd selfhosted && ./ladder.sh --full` | 39 pass / 0 fail / 0 known-div |

三线关系:阶梯证明基线(selfhosted)未破;smoke 证明新树与基线逐字等价;
suite 证明语言能力面对齐可执行规范。**改动 compiler/src 后三线都要跑。**

## 6. 不变量与坑(实现者必读)

以下均为本轮实战踩中,记录在此避免复发:

1. **调用 arity 已有编译期闸门(2026-09-08)**:sem 的 E2010 检查 bare 调用
   实参数(成员调用面留类型检查 v1);此前实参错位静默误绑,`ty_head` 的缺参
   or3 靠发射器按 0 补齐掩蔽多年,已按既有可观察行为忠实修复(eval/ev2/cc 三处)。
   写跨函数调用后仍必须逐参核对。类型检查 v0(2026-09-09)再落 E2020 全量
   Ident 读解析与 E2010 基础统一(保守可证:任一侧类型未知即放行)。
2. **无 `||` 中缀**:Ctron 侧 parser 只有 `&&`(p_and),逻辑或一律 `or2/or3`。
3. **env 前插式**:`env_add` 把新绑定放在头部 → "新增段"是 `[0, extra)`,
   逆声明序遍历 = 下标**升序**(drop 触发曾因此扫错端)。
4. **M 单元身份**:`vdeep`/`bind_of` 克隆 U 时,Atomic/Global 的 M 单元必须原样
   传递(不重建),否则 store 写进副本、别名断裂(drop 观察失效即此因)。
5. **记录槽序两套**:`e4(fl, env, vv, out)`(表达式)与 `s4(fl, env, out, vv)`
   (语句)——`run_block` 返回 s4,进表达式上下文必须 `e4(br[0], br[1], br[3], br[2])`
   转换,槽序错=原生段错误。
6. **out 全程穿线**:print/println 只追加 out 串,由驱动最后统一打印。
   新增入口/分支必须把 out 接回(spawn 曾丢弃闭包 out,test 模式曾被
   run_tests 丢弃,均为静默无输出的形态)。
7. **`a[i] {` 前瞻门控**:postfix `[..]` 后紧跟 `{` 曾被无条件判为泛型 TypeArgs,
   吞掉 if/while/match 条件(条件上下文 al="0")——已按 allow_struct 门控;
   新增语法冲突时优先检查 al 线程。
8. **ARM64 除零不陷阱**:硬件除零静默回 0,检查算术必须显式判零。
9. **`||` 之外的转义**:eval 源内字符串写 `{` 要用 `\{`(插值转义,镜像词法器),
   `\n` 用 `bs()`/`bsn()` 拼——发射器源(trans.ct)尤甚。
10. **层级纪律**:eval.ct 自身运行在宿主语义上(直接 `.push`/`.len`/索引),
    而它操纵的被解释值走值模型(标签 + `call_mem` 分发)。改代码前先分清
    "这段是宿主语义还是值模型"。
11. **单行 if 块内不容 `return`**(宿主解析器口径,赋值/表达式可以):含
    `return` 的块一律多行展开;`;` 与 `\}`(只有 `\{` 是合法转义)同样会被
    宿主拒绝——自举双解析器差异以宿主为最严口径。

## 7. 边界与挂账

- 后端是 **gcc**(发射 C),不直接出机器码。
- 发射器能力面 = `selfhosted/fixtures/trans_v0–v5` + 编译器自身;"编译任意
  Ctron 程序"(枚举/闭包/并发/值位 match 泛化)按 selfhosted 排期;
  struct 值类型(2026-09-09)与值位 if/插值(2026-09-09)已入面;struct 的
  print/to_string/字段赋值、非纯表达式分支的值位 if 命中即 panic。
- 节点形态不对称备忘:if 的 else 分支被 p_if 包为 BlockExpr(then 是裸 Block)
  ——消费 If 节点的代码必须两形态都处理。
- 语言无 break/continue(保留字无实现)——解析器静默收为 Ident,运行期才炸
  (2026-09-09 两度踩之);or2/or3 是函数不短路,副作用条件必须拆开写。
- compiler-c Makefile 不追踪头文件以外的新依赖变更,改 .c 后 make 可能
  "Nothing to be done"——touch 源文件强制重链。
- 模块加载器(2026-09-09)对无 use 源逐字节无感(金样安全);加载失败即 rc=1
  (visibility/circular 用例即此形态)。
- span 标注(2026-09-09):节点末槽 = 行号串(nstamp 追加),**必须在节点构建
  完成后追加**——插在中部会把既有槽位整体后移(sem/eval/trans 全按固定下标读);
  诊断内部 "CODE@行",文本面 strip_at 剥离保持逐字,JSON 面消费。
  W8030 暂无行(绑定行未入表),探针回退。
- 踩坑复记:发射器新代码两度踩 `||`(自举 parser 静默、宿主 parser 拒绝)与
  单行 if 内 return——宿主 seed 永远是最严口径,build 通过 ≠ 宿主可跑。
- 并发为顺序化模拟:spawn 即刻完整执行,满发送立即 `Err(ScopeCancelled)`;
  与"结果与调度顺序无关"的测试面语义一致,真线程竞争不可观察。
- U64 大数字面量(超出 I64 表示)不在宽度域内(03b 仍依赖 `as[]` 全集,挂账)。
- `roadmap/`(红=规范锚)与 `modules/`(多文件包)不在单文件一致性口径内。
- suite 口径盲区:自举单步 run 在文件含 fn main 时不执行 test 块(宿主
  check/test 两阶段执行)——test 块断言由宿主侧兜底,自举侧仅覆盖 main 路径。
- `or` 中缀(§4.4,2026-09-09)双侧已对齐:仅解包左侧,默认值原样返回;
  发射器(trans)对 Binary "Or" 仍走兜底 panic(发射面挂账)。
- comptime v0(2026-09-09):const/static-let 在 check 面按声明序试求值(镜像
  statics_env,前向引用同样失败);无步数预算,comptime fn 死循环挂起编译;
  求值 panic 为宿主级原文中止(不经 Ctron 流,E6010 留给预算超限)。

## 8. 速查

```bash
make -C compiler-c                        # 首次:构建宿主 seed
compiler/build.sh                         # 拼接三产物
compiler/native.sh                        # 自举:出 bin/ctron-cc + bin/ctron-emit
compiler/ctc.sh <in.ct>                   # 运行
compiler/ctc.sh check <in.ct>             # 检查(decls=N)
compiler/ctc.sh emit <in.ct> [out.c]      # 发射 C → cc -O2 out.c
compiler/test/smoke.sh --full             # 自举验收 18 项
CTRON_CC=<bin> python3 compiler/test/suite.py   # 一致性套件(可换被测编译器)
cd selfhosted && ./ladder.sh --full       # 旧快照完整阶梯 39 步
```
