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
| `input_*.ct` | 差分夹具 | — |

## 用法(宿主 = C 版 ctronc)

```bash
cd compiler_c && make          # 构建宿主
# 解析任意 Ctron 测试码(结构树 → C-AST v1)
./build/ctronc parse-ct <file>
# 语义检查(12 项)任意测试码
./build/ctronc parse-ct <file> ../selfhosted/sem_chk.ct
# 词法
./build/ctronc parse-ct <file> ../selfhosted/lex_num.ct
# 模块级检查(示例包)
./build/ctronc run ../selfhosted/pkg_chk.ct
```
> `parse-ct` 会把模块里 `read_file("...")` 的目标自动替换为 `<file>`。

## 验收

```bash
make -C compiler_c test    # 全量差分(suite_run/suite_diff 直接跑本目录模块)
```

## 自举状态与下一步

- 词法 / 全语法结构化解析 / 单文件语义(12 项)均在 Ctron 侧达成,全部以 C 版契约差分锁定;
- 下一步:模块级其余(caps E4010/comptime 预算/use 可见性)、Ctron 侧执行层(test 块运行)、统一的 Ctron `cc` 驱动
  (parse→check 串成一个模块入口),之后把运行也交还 Ctron 侧。
- 详细路线见 `docs/superpowers/plans/2026-09-05-c-bootstrap.md`。
