# Ctron 编译器 —— Rust 参考实现

> **范围**:本 crate 是 Ctron 语言的**独立完整 Rust 实现**(零外部依赖,Rust std only,edition 2021,MSRV 1.75)。
> Ctron 自身实现编译器(自举/自宿主)由独立工作流负责,**不在本 crate 范围**。
> 与 C 版实现(`compiler_c/`)分治:语言规范(`docs/spec/` v0.5)与一致性语料(仓库根 `tests/`,61 文件)共享,工程互不依赖。

## 状态总览

| 组件 | 入口 | 状态 |
|---|---|---|
| 词法器(§1.3–1.6) | `src/lexer.rs` | ✅ 61 文件零诊断 |
| 解析器(§1.7 EBNF 全语法 + §1.8 消歧) | `src/parser.rs` | ✅ 61 文件诊断矩阵 |
| 语义检查(Send 三检查点/穷尽性/效果/comptime/能力) | `src/check.rs` | ✅ marker 全对齐 |
| 解释器(全语言树行走) | `src/interp.rs` | ✅ 61 文件全量运行 |
| **原生转译后端(Ctron → C11)** | `src/trans.rs` | ✅ **33/33 行为语料原生编译运行,逐一与解释器差分一致** |
| 单态化(泛型/trait 对象形参) | `src/trans.rs` | ✅ 调用点惰性例化 |
| 并发运行时(spawn/Channel/Mutex/scope 取消) | `src/trans.rs`(pthreads) | ✅ 真线程 |
| 双实现 AST 差分 | `tools/ast_diff.py` | ✅ 与 C 版 61/61 一致 |

**验证基线**:全套 8 个 cargo 测试目标全绿;原生差分套件下限锁定 **33/33**(任何回退即红)。

## 命令行

```
ctron lex <file>              # 词法诊断
ctron parse <file> [--ast]    # 解析诊断 + 确定性 AST 文本(跨实现差分产物)
ctron check <file> [--profile bare|web|full]
ctron run <file>              # 解释器执行全部 test 块
ctron trans <file> [--with f.ct ...] [-o out.c]   # 转译为 C11(gnu11),多文件合并
ctron build <file> [--with f.ct ...] [-o bin]     # trans + cc 一行得到原生二进制
                                                 # (自动链接同包 c_src/*.c,FFI)
```

## 转译后端语义契约(§3.6 等)

生成 C 的每条语义都与**本 crate 解释器**逐语料差分验证(解释器裁决 = 期望):

- 整数 `__int128` 统一承载;宽度只体现在运算检查:检查算术溢出 panic、
  `+%`/`-%` 二补回绕、`as[T]()` 截断、除零;结果宽度取左操作数标记;
- 字符串 `char*` 载体(进程期分配),插值/方法/UTF-8 边界切片;
- 数组句柄共享(对齐解释器 `Rc<Vec>`)、struct 值拷贝、class/Box 指针;
- 和类型 tagged union(Option/Result/用户 enum)、`?` 传播、context/AnyError 错误链;
- 捕获闭包(env 结构体)、spawn/pthread 有界通道、Mutex 单元格、scope 取消广播、
  任务 panic 经 setjmp 在 join 边界捕获;
- 泛型/trait 对象形参按实参具体类型惰性单态化;`derive(Show)` 综合。

## 测试矩阵(`cargo test`)

| 套件 | 覆盖 |
|---|---|
| 单元测试(48) | 词法/解析/检查/转译锚点 |
| `lex_suite` / `parse_suite` / `check_suite` | 61 文件诊断矩阵 |
| `run_suite` | 61 文件解释器运行期望 |
| `native_suite` | 33 语料:转译 → cc → 原生运行 vs 解释器差分(下限 33 锁定) |
| `tools/ast_diff.py`(仓库根) | 与 C 版前端 AST 61/61 一致 |

## 里程碑文档

- `docs/superpowers/plans/2026-09-04-p1a-lexer.md`、`2026-09-05-p1b-parser.md`、`2026-09-05-p1c-checker.md`
- `docs/superpowers/plans/2026-09-06-p1e-trans.md`(转译后端 P1-E①–⑯ 全记录)
- `docs/superpowers/plans/2026-09-05-bootstrap-ast-diff.md`(双实现一致性闭环)
