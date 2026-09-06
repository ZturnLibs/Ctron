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
- C9c ✅(`cc.ct` 统一驱动 parse→sem12→run;seq=9;提交待本批次尾)。验收 220 cases + suite_run 16 files。
- 下一批见计划 18y 意向:单文件语义 12 项之外扩面 / 运行域对齐 / selfhosted 独立工具链。
- C9d① ✅(match+数组;seq=5b)提交 1c83147;C9d② ✅(Option/Result tag+`?` Try;seq=5c 222 cases)。
- 注意:并行 C10-a(trans)流已把 compiler_c/Makefile 接上未落盘的 tests/suite_trans.c → 整体
  `make test` 暂不可用;验收请逐 suite 构建运行(或等其落盘)。勿改其 Makefile。
- C9d③ ✅(struct/枚举域;seq=5d 223 cases)。
- C9e① ✅(cc.ct 快照刷新至 C9d + UFCS;seq5e/seq9-cc2,225 cases)。
- C9e② ✅(闭包/fn 值;seq5f,226 cases)。
---
## 更新记录(19d session,C9f①)
- **List 索引写缺陷已修复**:宿主 a3bf0b7 修掉 ST_ASSIGN 的 V_LIST 写透上界误用,现 `l[i]=v`
  可用且写透共享后备(探针验证);上文 18x 记录的"List 索引写有缺陷"钉子过时。
- C9f① ✅(`ev2.ct` List 域引用语义;seq5g input_ev2g.ct,227 cases)。客体 List = 宿主 List
  对象,push 原地变/别名可见/into_gc 深拷贝,与 rt.c listnode 语义对齐。
- 新钉子:①宿主 **for-over-list 不支持**(rt 仅 range/数组/tuple;ST_FOR 对 V_LIST 报
  "for 需要 range 或数组");②println(List) → `<value>`;③List 构造 `List[T]()` 同参忽略恒空表。
- **竞态实录(重要)**:本批提交 9638776 时把并行 session 当时在磁盘上的 trans.c WIP 一并
  add 了(未先 `git diff` 自查);对方随后提交 C10-c,我方 reset 误撤其提交,对方又补 docs
  提交恢复。教训:**提交前必须 `git status` + `git diff --stat` 逐文件自查,只 add 自己泳道
  的文件;发现混入且 HEAD 已被并行方推进时,勿再做历史手术,现场移交并在文档留痕**。
- 并行 C10 流 trans.c WIP 可能短暂阻断 `make build/suite_diff`(同一 CORE 链接);应急:
  `git show HEAD:compiler_c/src/trans.c > /tmp/trans_stable.c` 用稳定版链接验收,不改其文件。
- 下一批:C9f② trait/impl 方法域(用户类型方法调用/trait 默认方法),或 cc.ct 快照刷新并入 C9f①。
- C9f② ✅(trait/impl 方法域;seq5h input_ev2h.ct,228 cases)。分发序镜像 rt:
  内建 → 类方法(找不到即 panic 不落 UFCS)→ UFCS;Member:字段 → impl prop → panic。
- 新钉子:①宿主解析器**无 inherent impl**(`impl Type {}` 不支持,必须 impl Trait for Type);
  ②self 可变方法挂账 C9g(类原地写 vs 结构体重建的写语义区分);③内建 to_string/slice
  未按接收者种类门控(夹具避开)。
---
## 更新记录(19f session,C9g)
- C9g ✅(绑定克隆与原地写;seq5i input_ev2i.ct,229 cases)。bind_of 镜像 env_let 的
  克隆决策(结构体非类深克隆入槽,类/List/标量保引用);成员写改 u_set_ip 原地透
  (self 可变方法由此可用);vdeep 按种类分发重写。
- **竞态镜像重演**:我方未提交的 C9g 三文件(ev2.ct/input_ev2i.ct/suite_diff.c 注册行)
  被并行 session 以 40cdd0c(P1-D 提交)一并落库 —— 双向竞态成立。裁决:不再做历史
  手术,以本条 + 计划 19f 留痕;后续双方都应:**改完即小步提交,add 前逐文件自查**。
- 新钉子:①**字符串载荷不可索引**(裸槽 v[1] 做 it[0] 即宿主 panic"索引目标非数组";
  本批 vdeep 初版踩中,已修);②for 迭代绑定不克隆(镜像 rt ST_FOR 棗写);③结构体
  右值字段写克隆(rt 774 行)挂账。
- 排障经验:suite_diff 计数不一致(O2 229 vs ASan 228)为陈旧二进制所致 —— ASan 二进制
  建于 suite_diff.c 中间态;**每次改套件用例后两个构建都要重建再对比**。并行方 trans.c
  WIP 若阻断构建,用 `git show HEAD:compiler_c/src/trans.c` 稳定版链接验收。
- 下一批:cc.ct 快照刷新并入 C9f/C9g,或单文件语义扩面,或 selfhosted 独立工具链。
- C9h ✅(cc.ct 快照刷新至 C9g + 独立驱动 cc.sh + seq9 input_cc3;230 cases,
  make test 端到端恢复绿)。selfhosted 自此自带工具链入口:
  `selfhosted/cc.sh <input.ct>`(正例运行/负例编译期拦截)。
  归属勘误:C9h 四文件(cc.ct/cc.sh/input_cc3.ct/suite_diff.c)被并行 session
  卷入 83a81bd 落库,设计裁定与验收以 921b0d7 文档为准 —— 竞态第三次,判定为
  对方工作流固定行为,我方对策:验证完成后立即提交,不跨验证窗口攒批。
- 独立化路线(用户口径:不依赖不关注其他实现,专注自举版自身):
  ①cc 语义面扩面(12 项之外);②求值器运行域补尾;③**自译化阶梯**:cc.ct 解释
  cc.ct(cc.sh 嵌套自举),验证自足性;④差分脚本从 compiler_c/tests 迁移/复制到
  selfhosted/ 本地,验收不再依赖宿主管的套件源码。
- C9i① ✅(自译化尝试暴露真实缺口并修复):**Ctron scan 无换行抑制规则** —— cc.ct 自身
  源码用了 `&&` 领行续行,C 解析器接受、Ctron 解析器遇 NL 即断。修复:六模块统一安装
  filter_nl(镜像 C filter_newlines)+ p_file/p_block 停滞守卫(镜像 ensure_progress)。
  分歧挂账:①`{` 行尾抑制暂不启用(p_block 依赖该换行,启用即回归);②pkg_chk/parse_ast
  未加守卫。自编译阶梯:input_cc3/sem_chk/parsetree ✓(≤2s);ev2/cc 种子解释器资源边界
  (~90-120s 被杀)→ 下一阶梯 C9i②:种子解释器性能(或自举二进制替代种子)。
  验收:make test 全量绿(含并行新增 corpus_trans 17/17)+ suite_diff 230(O2/ASan 同数)。
- **⚠️ 负载警示(重要)**:本轮后半段机器 load average 6.16(并行 session 并发构建),
  全部自译化计时数据不可信:20 行文件实测 177s(纯 CPU 争抢),8s 超时被误判为挂起。
  **下 session 首要先确认负载,再重测全部阶梯**。
- **确定性发现(不受负载影响,待查)**:cc_phase4(现行 cc + D 探针)上,ev2.ct 的
  p_file 产物 decl#102 是垃圾(名 = "NL"),sem_walk2 走该 decl 即崩(索引目标非数组)
  —— 即 **eval_call 区域的解析错位在 NL 过滤后依然存在**(非局部:eval_call 孤立解析
  秒过;仅在完整前缀后出错)。且 p_post 的 `[` 前瞻扫描(`while d > 0 { toks[q]... }`)
  对括号密集源码是 O(N×depth) —— 强烈疑似平方级,与负载叠加即成被杀假象。
  下一步:①静机器重测 ev2 自编译(区分真慢/错位);②修 p_post 前瞻(预计算括号配对
  或缓存匹配位置);③用 TRV 阈值探针法定位垃圾 decl 的确切吞入起点。
