# Lane: feat/cli-test-fmt — R-P2c `ctron test` + R-P2d `ctron fmt`

日期:2026-09-12 · 线:compiler-rust(R 线唯一产品线) · 分支:`feat/cli-test-fmt` · worktree:`/Users/zyj/Zturn/Ctron-cli`

## 与其他 lane 的分工(并行协调)

| lane | 占用 | 本 lane 回避 |
|---|---|---|
| `feat/closure-abi`(Ctron-clo worktree) | compiler/src 的 eval_*/trans_conc/trans_ty/driver_emit + compiler-c/rt_eval.c,闭包 ABI 32→64 | 不触碰自举编译器与 C 参考实现 |
| std 扩面(已落 main) | compiler/test/stdpkg/std/*.ct、examples | 不触碰 std 内容与 smoke 段 |

本 lane 只动:`compiler-rust/src/{main.rs,lib.rs,interp.rs,testing.rs新,fmt.rs新}`、
`compiler-rust/tests/{test_suite.rs新,fmt_suite.rs新}`、`docs/fmt-spec.md新`、本 plan。
对 interp.rs 的改动仅限 `run_tests` 追加 `fn test_*` 发现(闭包 lane 不涉 R 线 interp,无冲突)。

## R-P2c `ctron test`(roadmap S)

交付:`ctron test [file|pkg目录] [--filter pat] [--format=json] [--deterministic]`。

- **发现**:`Decl::Test` 块 + 零参 `fn test_*`(统一进 `interp::run_tests`,roadmap 要求复用收集逻辑)。
- **装配**:镜像 `interp::run_test_file`(sem::build_package + parse_file_public + Interp),新模块 `src/testing.rs` 出结构化报告;`run` 子命令保持原样(campaign.py 兼容)。
- **panic 标记**:文件级 `//@ panic:` 语义从 run 迁入——结果 panic 且消息含标记 → `panic-ok`。
- **--filter**:名称子串过滤。**--deterministic**:接受并文档化(interp 本征顺序执行;真效果随原生后端并发落地)。
- **pkg 模式**:目录(Ctron.toml + src/*.ct)按文件名序拼接为单源运行(v0 不去重跨文件 test 名)。
- **退出码**:0 全过;1 有失败/解析诊断;2 用法错。文本输出对齐 run(`ok/FAIL/panic-ok` + 汇总行);
  JSON:`{total,passed,failed,results:[{name,status,message?}]}`,status ∈ ok|fail|panic-ok。

## R-P2d `ctron fmt`(roadmap M)

交付:`ctron fmt [file|pkg] [-w|--check]` + `docs/fmt-spec.md`。

- **载体**:token 流重排(不动 AST),遵守 roadmap 指定的 token 层安全性。
- **核心不变式(验收机械化的关键)**:`lex(fmt(x)).0` 与 `lex(x).0` 的 **Tok 逐项相等**。
  依据 §1.6 过滤器(lexer.rs filter_newlines):
  - 行尾延续集(Comma/Assign/运算符/LParen/LBracket/LBrace/Pipe…)后的换行被滤除 → fmt 在 `{` 后强制换行,
    输出重词法化同样滤除 → 流不变;
  - 下一行以 `.`/二元运算符开头 → 换行被滤除 → fmt 的链断行(逐 Dot 看原始源 span 间隙含换行)重词法化同样滤除;
  - 幸存 Newline = 语句边界 → fmt 原样落为换行(空行按原始间隙 ≥2 换行展开为至多 1 空行)。
- **注释回贴**:词法器吞注释 → fmt 从源码 span 间隙回收 `//`/`///`/`//@`,
  按注释前是否隔换行分为行尾注释(前置单空格)与独占行注释(当前缩进整行);文本逐字保留。
- **字面量保真**:Str/Int/Float 一律按 span 从原源切片输出(不做解码-再编码,插值/转义/后缀/进制零风险)。
- **空格表**:二元运算符/箭头/fat arrow 两侧空格;`,`/`:` 前无后有空;`.`/`?`/`#`/`@` 紧贴;
  一元 `-`/`!`/`&` 紧贴(按前记号是否操作数收尾判定);`(`/`[` 后与 `)`/`]` 前紧贴;
  `{` 前恒有空格,`{}` 空块不拆行;Pipe 闭包 `|x, y|` 参数紧贴。
- **缩进**:4 空格;LBrace 深度 +1;括号内幸存换行按括号深度局部缩进(v0 不按行宽折行,fmt-spec 记录)。
- **验收**:tests/fmt_suite.rs —— 逐规则单测 + 全语料(`../tests`)三断言:
  ① Tok 流逐项相等 ② 幂等 fmt(fmt(x))==fmt(x) ③ 行为矩阵不变(同 run_suite 口径:
  行为文件 run_test_file 结果逐项相等;neg/lint 不运行;10_web 跳过)。

## 基线与已知账

- 基线:cargo build 4.3s 绿;48 lib 测试绿;**两处先行失败均与本 lane 无关,同一根因(`or` 中缀 R 线未实现的语料漂移)**:
  - check_suite::all_suite_files(01h_str_plus 报 println 未解析;04c_or_infix 报 or/&&/to_string);
  - run_suite::all_behavior_files(04c_or_infix 三个 test 断言失败)——已在 main 检出复现,非本 lane 引入。
  登记待「R 线语料同步」切片处理;本 lane 不得使其恶化(现状:均持平)。
- `--deterministic` v0 为接受即文档化的占位(interp 单线程顺序执行,天然确定)。
- 语料门禁采用 roadmap 原定三断言(幂等/AST 等价/行为矩阵),不用「Tok 流严格相等」:
  权威格式把 `}` 换行 `else` 归一为同行 `} else`,该处流不等但解析与行为等价。

## 完成定义

cargo test 全绿(除上述先行失败持平)+ 三语料断言全过 + main.rs usage 更新 + 本 plan 与 fmt-spec 入册 + 分支提交。

## 落地结果(2026-09-12)

- **R-P2c ✅**:`interp::run_tests` 扩展零参 `fn test_*` 发现;`src/testing.rs`(TestReport/panic 标记/过滤);
  `ctron test <file|pkg> [--filter|--format=json|--deterministic]`;tests/test_suite.rs 14/14。
- **R-P2d ✅**:`src/fmt.rs` token 流序列化器(布局权威=原始源码间隙,§1.6 过滤流丢失多行性的
  结论落地为统一规则);`ctron fmt <file|pkg> [-w|--check]`;`docs/fmt-spec.md`;
  tests/fmt_suite.rs 23/23(逐规则 13 + CLI 3 + 全语料三断言:幂等/AST 等价 61+ 文件/行为矩阵 36 文件)。
- **回归**:lib 48/48、lex/parse/roadmap/native 全绿;run_suite/check_suite 失败为 main 先行旧账(已在上文登记,持平)。
- **语料门禁实战**:全语料 54 文件 48 个待规范化(行尾注释对齐空格折叠为主);
  门禁抓到并修复真 bug:`return!(...)`/`&&!b`——前缀 `!`/`&` 紧贴规则误置于 cur 侧(补回归测试)。
- **遗留**:链断行采用「逐 Dot 原始间隙」判定(首点链整链统一断行为 v1 选项);
  `--deterministic` 占位;pkg 模式跨文件 test 名不去重;R 线语料 `or` 中缀漂移待独立切片。
