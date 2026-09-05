# Ctron 编译器 C 版(C1 词法 ✅ / C2 解析 ✅ / C3-a 语义 ✅ / E3040 分配效果 ✅)

> **分支**:`discuss-c-implementation`。**决策记录**:Ctron 存在两套独立、各自完整的编译器实现——Rust 版(`compiler/`)与 C 版(`compiler_c/`),互不依赖;两者共享**语言设计**(`docs/superpowers/specs/…ctron-language-design.md`)、**规范**(`docs/spec/` v0.5)与**一致性语料**(仓库根 `tests/`,61 文件)。最终自举目标不变:以 C 版为种子编译器,后续用 Ctron 自身实现 Ctron。

## 已交付里程碑

### C1 词法器 ✅
记号模型(§1.3–§1.5)、换行显著性过滤(§1.6)、E1001 循环错误恢复;58 单元断言 + 61 文件零词法诊断。详见 `docs/superpowers/plans/2026-09-05-c-lexer.md`。

### C2 解析器 ✅
规范 §1.7 EBNF **全语法**递归下降 + §1.8 消歧 + §4 优先级,产出纯数据 AST;诊断 E1001 / E3030(static var 恢复)。

- **集成验收**(与 Rust 版同一口径):61 文件中仅 `01c_parse.neg.ct` 报 E1001、`06_static_var.neg.ct` 报 E3030,**其余零解析诊断**。
- 单元锚点:声明/类型三态(空=切片、单整型=定长数组、其余=泛型实参)、`]` 后跟 `(`/`{` 快速路径与顶层逗号回退、比较不可链、`else` 同行、赋值目标校验、2000 层 `(`/1500 层类型括号嵌套上限(嵌套过深 E1001,不栈溢出)。
- `ctronc parse <file> [--ast]`:AST 确定性文本(自举差分产物形态,C 版契约见下)。
- ASan/UBSan 全绿;`make test` 全绿(四件套)。

## 目录与构建

```
compiler_c/
  Makefile              # make / make test / make clean
  src/arena.{h,c}       # 块链 bump 分配器:编译器对象整体一次释放
  src/token.{h,c}       # 记号模型
  src/lexer.{h,c}       # 词法器 + §1.6 换行过滤
  src/ast.h             # C 版纯数据 AST(字段即契约)
  src/parser.{h,c}      # 递归下降解析器(parse_src 入口)
  src/ast_show.c        # AST → 确定性 Debug 文本
  src/main.c            # CLI: ctronc <version|lex|parse [--ast]>
  tests/test_lex.c      # 词法单元(58 断言)
  tests/suite_lex.c     # 61 文件零词法诊断
  tests/test_parse.c    # 解析单元(锚定行为)
  tests/suite_parse.c   # 61 文件解析分类验收
  src/sem.{h,c}         # 语义检查(C3-a)
  tests/suite_sem.c     # 61 文件语义 marker 评分
```

构建环境:仅 libc,C11(`cc`);零外部依赖。验收命令 `make test`。

## C 版架构决策(贯穿后续里程碑)

- **内存**:单 arena 块链分配;AST 与记号载荷字符串整体一次释放;解析期读取记号文本后再释放词法 arena。
- **AST**:判别式结构体 + arena 数组;解析器与词法器各持诊断队列、按序合并(词法在前)。
- **嵌套深度上限 256**:类型与表达式共享深度计数(与语料/参照行为同源);超限产 E1001"嵌套过深",恢复路径不栈溢出。
- **AST Debug 文本**:`ctron_file_show` 单行 Rust-Debug 同族格式(变体名 + 字段),确定性输出 = C 版自举差分产物契约(C-AST v1)。
- 错误恢复:顶层/体内循环全部带停滞守卫(`ensure_progress`),每条错误路径都推进。

### C3-a 语义检查 ✅
单文件 11 项检查(Send 三检查点/match 穷尽/pure/comptime/no_spawn/own 块 move 与 gc-mut/浅拷贝 lint/must-use lint);`suite_sem` 读 marker 对 61 文件诚实评分,行为文件零诊断。详见 `docs/superpowers/plans/2026-09-05-c-sem.md`。

## 后续里程碑(C 版路线,独立推进)

| 里程碑 | 内容 | 出口 |
|---|---|---|
| C3-c | 模块级:孤儿/循环/导入可见性/caps/comptime 预算(读 `Ctron.toml`) | 全部 neg/lint 语料命中(含 `tests/modules/`) |
| C4 | 执行层(CVM):`ctron test` 跑行为/panic 语料 | 行为/panic 测试运行通过 |
| C5 | 自举种子就绪:ctron0-C 可编译 Ctron 写的模块 | — |

计划文档:C1 `docs/superpowers/plans/2026-09-05-c-lexer.md`;C2 `docs/superpowers/plans/2026-09-05-c-parser.md`;C3-a `docs/superpowers/plans/2026-09-05-c-sem.md`。
