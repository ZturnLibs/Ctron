# selfhosted —— Ctron 自举编译器(用 Ctron 写的编译器,独立目录)

> Ctron 自举的唯一目录:所有 Ctron 编译器模块、夹具与差分管线都以本目录为源。
> C 版(`compiler_c/src`)仅作为宿主运行器与差分 oracle,保留不清理。

## 组成(按管线顺序)

| 模块 | 阶段 | 状态 |
|---|---|---|
| `lex_small/kind/adv/corpus/float/pay/str/num.ct` | Ctron 词法器 v1→v5 | 51 语料 payload 全字一致 |
| `parsetree.ct` | 全语法结构化 AST(节点=List 数据树)+ 走树打印 C-AST v1 | 49/49 语料逐字节一致 |
| `parse_ast.ct` | 文本 AST 差分轨道(备份) | 49/49 |
| `sem_chk.ct` | 单文件语义检查 **12 项全集**(W8010/W8020/E4030/E3020/E3031/E3060/E4020/E6020/E3010/E3050/E2030/E3040) | 49/49 与 C 逐字一致 |
| `ev_num.ct` | 执行种子:树上数值求值(17/256/-15/3) | C9b-0 |
| `ev2.ct` | **Ctron 求值器扩面**:纯函数式树行走解释器(Bool/Str/变量+块作用域/if/while/for/递归/test+assert/输出),主模式+测试模式自判 | C9b① seq5/6 与 C rt 逐字一致 |
| `input_*.ct` | 差分夹具(含 `input_ev2.ct` 主模式、`input_ev2t.ct` 测试全过、`input_ev2tf.ct` 断言失败) | — |

## 用法(宿主 = C 版 ctronc)

```bash
cd compiler_c && make          # 构建宿主
# Ctron 求值器(ev2.ct)解释其 read_file 目标 —— 主模式/测试模式按内容自判
./build/ctronc run ../selfhosted/ev2.ct
# C 宿主参考运行同一输入(执行差分 oracle)
./build/ctronc run ../selfhosted/input_ev2.ct
# 其余模块同理(其 read_file 目标由 suite_diff/suite_run 按夹具换靶)
./build/ctronc run ../selfhosted/sem_chk.ct
./build/ctronc run ../selfhosted/pkg_chk.ct
```

## 验收

```bash
make -C compiler_c test    # 全量差分(suite_run/suite_diff 直接跑本目录模块)
```

## 自举状态与下一步

- 词法 / 全语法结构化解析 / 单文件语义(12 项)均在 Ctron 侧达成,全部以 C 版契约差分锁定;
- C9b①(Ctron 求值器扩面)已交付:`ev2.ct` 树行走解释器覆盖 Bool/Str(含 `{expr}` 插值)/变量+块作用域/
  if·while·for(range 含 ..=)/函数调用与递归/print·println/assert·assert_eq·assert_ne(test 块运行语义);
  suite_diff seq=5(主模式)·seq=6(测试模式,含断言失败消息)对同一输入与 C rt 逐字一致;
- 下一步:C9b② comptime 预算 E6010(求值步数上限,无限递归 spin 命中)、C9c 统一 cc 驱动
  (parse→check(12 项)→run 单入口),之后把运行域逐块对齐 C rt 其余文件(test/循环/变量已覆盖)。
- 详细路线见 `docs/superpowers/plans/2026-09-05-c-bootstrap.md`。
