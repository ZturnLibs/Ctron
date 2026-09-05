# Ctron 自举 交接文档(新 session 从这里开始)

> 创建时间:2026-09-05(长 session 后)。新 session 请先读本文件,再读
> `docs/superpowers/plans/2026-09-05-c-bootstrap.md`(里程碑全史)与 `git log --oneline -25`。

## 0. 仓库与命令
- 仓库根:`/Users/zyj/Zturn/Ctron`,分支:`discuss-c-implementation`
- 构建/验收:`cd compiler_c && make test`(约 10 秒;208 diff cases + 若干 suite)
- ASan:`cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc -o build/asan/suite_diff tests/suite_diff.c src/*.c && ./build/asan/suite_diff ../selfhosted`
- 直接跑 Ctron 模块:`cd compiler_c && ./build/ctronc run ../selfhosted/<mod>.ct`
- Ctron 模块处理任意文件:`./build/ctronc parse-ct <file> [module]`
- **当前工作区应干净(仅历史提交)。**

## 1. 战略(用户口径,必须遵守)
1. **Ctron 自举 = 用 Ctron 实现 Ctron 编译器**,逐模块与 C 版契约差分锁定。
2. **不要管理/重构/清理 C 版**(`compiler_c/src` 只是宿主与 oracle,保留)。
3. **Ctron 自举代码全部放根目录 `selfhosted/`**(`compiler_c/selfhost` 已删除,勿重建)。
4. suite_run/suite_diff 直接以 `../selfhosted` 为模块根;模块内夹具路径字面量一律
   `../selfhosted/input_*.ct`;suite_run 跳过 `input_*` 数据文件(勿再犯)。
5. 每步交付:新增/改动 → 立刻跑 `make test` + ASan → 更新计划文档(18x 项)与
   `selfhosted/README.md` → 提交(信息含里程碑代号,如 C9b①)。

## 2. 现状(已交付,全部验证)
- 词法:`selfhosted/lex_*.ct`;结构树解析 `parsetree.ct`(49/49 语料与 C-AST v1 逐字节);
  文本轨道 `parse_ast.ct`;单文件语义 12 项 `sem_chk.ct`(49/49 逐字);
  模块级 `pkg_chk.ct`(E5010/E5020/E2020/E4010 负例命中,行为包零误报);
  执行种子 `ev_num.ct`(树上数值求值,17/256/-15/3 验证)。
- 宿主已扩展:`ctronc parse-ct`(通用 read_file 字面量替换)、rt 内建 `read_dir(Str)->Str?`。
- 验收口径:`make test` = suite_diff 208 cases(词法 51 + parse 文本 49 + 树 49 + 语义 49 + 夹具)+ suite_run 14 files + 其余套件;ASan/UBSan 绿。
- 里程碑索引:计划文档第 18a–18u(C5g→C9b-0);README 里程碑表同步。

## 3. Ctron 语言钉子(写 Ctron 模块时极易踩,务必牢记)
- 无 `||` → `or2(a,b)`/`or3(a,b,c)`(德摩根)。
- 无 `;`/`continue`/`break`;语句靠换行;`else` 必须与 `}` 同行(规范:先写
  `} else if … {` 整行风格,避免单行 if 后另起 else)。
- 字符串字面量里 `{` 必须写 `\{`(否则当插值);写完新模块**跑一次花括号转义器**
  (对字符串内未转义 `{` 补 `\`);`\u{4E2D}` 类转义勿被转义器破坏(它会把 `{` 加 `\`)。
- List/Atomic 可作可变共享状态(游标用 `Atomic[I32]`;List 索引写透在 rt 有缺陷)。
- 树节点 schema:child[0]=tag;绑定模式 tag 是 `PatId` 非 `Ident`;`Test` 节点只有 3 子
  (取 body/ps 前先判 tag);Message kind 用根对象名(成员调用取 obj 底链)。
- 数值/字符串比较在部分路径对子节点元素失效 → 自写 `seq(a,b)`(逐字节)兜底。
- Ctron 支持:递归/闭包/List push·len/if·while/BlockExpr/own/scope 等(runtime 已全量)。

## 4. 下一批任务(按序,单次 session 做 1–3 条并各自绿+提交)
1. **C9b① Ctron 求值器扩面**(最大价值):在 `ev_num.ct` 基础上加
   Bool/比较→Int、Str(含 `Str{parts}` 文本求值)、let 变量+块作用域、if/while/for、
   函数调用与递归(自建调用栈,fib/阶乘)、`test` 块+`assert_eq` 运行语义、输出;
   对齐 C 宿主 rt 已覆盖的 34 文件运行域。验证方式:对同一 Ctron 程序,让 Ctron 求值器
   输出结果 与 `ctronc run`(C rt)结果一致(差异即钉子,逐个裁定)。
2. **C9b② comptime 预算 E6010**:comptime 求值设步数上限(计划内模块级),
   无限递归 spin 用例命中。
3. **C9c 统一 cc 驱动**:在 `selfhosted/` 组装一个模块:parse→check(12 项)→run
   单入口(`ctronc parse-ct <file> ../selfhosted/cc.ct`),输出与 `ctronc check`+`run` 对齐。
4. 之后可考虑把 `selfhosted/` 提升为独立工具链(自带驱动/夹具/差分脚本),C 版仍保留。

## 5. 检索入口
- 里程碑与设计:计划文档(18a–18u 最新为 C9b-0)、`selfhosted/README.md`、`compiler_c/README.md`
- C 版 oracle 语义(sem/parser/rt 具体规则):`compiler_c/src/{sem,parser,rt,pkg}.c`
- 现有 Ctron 模块即最佳“写法范本”:`selfhosted/sem_chk.ct`(语义全集)、`pkg_chk.ct`(tok 提取)、`ev_num.ct`(求值雏形)

---
## 更新记录(18w session)
- C9b① ✅(`ev2.ct` 求值器;suite_diff seq=5/6 执行差分;提交 e468f22)
- C9b② ✅(`pkg_chk.ct` 升级为 C pkg 逐字 oracle + E6010 comptime 预算;seq=8 7 包;提交 7113c78)
- 验收口径升至 suite_diff 218 cases + suite_run 15 files,ASan/UBSan 绿。
- Ctron 钉子补充:**List 索引写有缺陷**(读=拷贝、`l[i]=v` 越界),可变结构请纯重建/Atomic;
  宿主是解释器,每层 Ctron 递归≈多 C 帧 → 深度受宿主栈约束(ASan 更严;comptime 预算深度取 40)。
- 仓库另有**并行工作流**(editors/lsp/trans.c 等,提交 b641b60 之后);若工作区有并行未提交改动,
  提交前只 `git add` 自己的文件。孤儿 C 改动(W8040/E2010、rt call_decl 调用方求值)在 `git stash@{0}`。
- 下一批:C9c 统一 cc 驱动(parse→单文件语义 12 项→run 单入口;把 sem_chk 检查段与 ev2 解释段
  并入一个自足快照,注意函数去重)。
