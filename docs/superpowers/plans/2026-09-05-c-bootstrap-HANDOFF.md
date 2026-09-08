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
- **C9i② ✅ 全深度自译化闭环**:allow_struct 线程化修正落地(patch_allow3:全局替换后重算
  span/自后向前替换/行数自校验——v1 的 span 失配是"线程无效"主因)+ p0 检查移入循环体
  (原误置循环顶=每轮强吞首 token)+ 新增内建 read_file/Atomic/load-store。
  **cc.ct 解释 cc.ct(163KB)parse+sem+运行嵌套 main,235s rc=0,输出与直接管线逐字一致。**
  自编译阶梯全绿:input_cc3(10)/sem_chk(109)/parsetree(57)/ev2(106)/cc(158),
  decl 数与 C 解析器逐一吻合。make test 全量绿(240+ 用例);ASan 230 全绿。
- 教训:①LitFs 守卫使误触发由死旋变"快吞静默 rc=0"——**验收必须对照 C 解析器的 decl 计数**;
  ②v1 span 失配:全局替换改变长度后必须重算 span,自校验(行数/fn 数)写进脚本;
  ③嵌套输出字面 \n = Ctron 字符串求值不展开转义(C 词法器转义),unesc 已补;
  ④负载污染:并行构建期间一切计时不可信,先看 uptime。
- **C9j① ✅ C 代码生成器 v0**(tools/trans_part.ct + genmod --trans + ladder 第 4 步):
  Ctron 写的代码发射器,trans_v0 fixture(add/fact/main)发射 C→gcc→原生执行,
  与解释执行逐字一致;make test 全量绿(trans 用 HEAD 稳定版链接,并行 trans.c
  中途重构暂不可编译,勿动其文件)。
- **钉子:发射的 C 必须 #include 独立成行**——单行拼接时 #include 吞掉整行剩余
  全部 token(链接 _main undefined 的假象);发射字符串的 { 写 \{;\n 用 "\\n"。
- 下一阶梯:C9j② 代码生成扩面(Str/Bool/比较逻辑已部分覆盖,补 struct/method/
  List)→ 产物落盘 gcc 自动化已在 ladder;C9j③ cc 语义面扩面(12 项之外)。
- **LitFs 守卫补遗(C9i①收尾)**:最小复现 `if <ident> <op> <ident> { 赋值 }` + 单行 fn 确定性挂起
  = p_pri StructLit 在 if 条件上下文误触发 + LitFs 无 #EOF 守卫。已落地 LitFs `#EOF` 退出 + `NL`
  跳过,三复现 0s 通过。allow_struct 线程化(C 正解)已实现到链路完整但 s6 仍挂(第二旋点未定位),
  已回退未提交;C9i③ = 静机器 + 探针重做 allow 线程化。ev2/cc 自编译被杀(129-141s)疑负载混叠,
  静机器重测后再定性。
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
---
## 更新记录(21x session,C9j④ 自发射收官)
- **C9j④ ✅ 自发射收官**:trans_part.ct 升 v3(List[Str] 引用语义/Atomic[I32]/索引读写/
  节点引用码 N/match-Option/read_file·byte_at·byte_slice·to_string/panic/函数原型前置/
  fn 体尾值)。**cc.ct(159 decls)→ 种子上 cc 前缀解析自身 → Ctron 代码生成器发射
  9,954 行 C → gcc 零错 → 原生自举 cc 解释 input_cc3 == 黄金逐字一致。**
  ladder 升 17 步(第 4 步 trans_v0–v3 全扫 + 第 6 步自发射收官),--full 全绿
  (自译化 127s);make test 全绿(corpus_trans 34/34 全转译);trans_v3 夹具新增,
  v0–v2 零回归。
- **发射器设计要点(写 v4+ 必读)**:①类型码 env "name:ty":i/s/b/f/L/A/N + a<N><码>(定长
  数组);N = 索引结果(节点引用,cc.ct 的 AST 节点以 List[Str] 名义流动,d[0] 是节点索引
  非字符索引);②列表元素槽一律 char*,节点 = 动态转型的 ctron_list*,调用点按形参码
  ct_arg_cast 插转换;③字符串发射用 ct_cstr(转义逐字透传,\{ 坍缩)—— **unesc 不可用于
  发射 C 字面量**(\n 变真实换行,预处理断行);④p_block 尾槽 = NL 折叠后紧跟 } 的表达式,
  完整语句后跟空行也入尾槽 → ct_body 按值类黑名单区分(ct_is_value);⑤实参缺额补 0
  (镜像 cc.ct ty_head 的 2 参 or3 潜伏 bug)。
- **既有问题记账(均非本批引入,已留痕计划文档 22a)**:①2931c15 模块拆分后 ASan
  suite_diff 在 rt_eval.c:360 栈溢出(纯 HEAD stash 验证复现;O2 230 全绿)——归
  compiler_c 泳道;ASan 命令需排除 src/main.c(拆分后与套件 main 重复符号):
  `srcs=(src/*.c); cc ... tests/suite_diff.c ${srcs:#*main.c}`(zsh)。
  ②cc.ct 4506 行 ty_head 对 or3 只传 2 实参,种子一旦执行必 panic(套件未触达)。
- **流程钉子**:①zsh 通配无匹配(如 rm $T/*.bin)会中止整条 && 链 → 批量清理用 rm -rf 目录;
  ②zsh 不做 $var 单词拆分,文件列表传 cc 用数组 ${arr:#pattern};③工作区若有并行
  session,写文件前先 stat mtime + git status(本 session 两次撞见对方热编辑,窗口期
  小步提交);④genmod 换靶只改模块副本,trans 模式发射产物 t_main 读磁盘 cc.ct 原始锚,
  需对产物再 sed(ladder 第 6 步固化)。
- 下一阶梯候选:C9j⑤ 原生自举 cc 扩验全部黄金面 + CTRON_SEED 换靶上位;值位 if/match
  泛化;struct/方法域发射(面向全语言);ASan 拆分回归修复(转交)。
---
## 更新记录(21y session,C9j⑤ 固定点)
- **C9j⑤ ✅ 自举固定点达成**:`ctron_len` 魔数动态分派修掉 N 值 `.len` 二义(env 元素
  真串 vs AST 槽节点;种子 rt 动态分派掩盖,原生编译即分叉)后实测:
  ①selfcomp.ct(编译器完整源,读自身)→ 种子发射 11,450 行 → compiler1;
  ②compiler1 编译自身 → **逐字节 == 阶段1 产物**;③compiler1 编译 trans_v3 →
  运行 == 黄金。另:原生自举 cc 解释器形态通过全部黄金面(input_cc/2/3 + 负例 rc1)。
  **自举三级闭环(自译化/编译/固定点)全部达成。**
- 收官工程待办(见计划 22b):发射 main 加 argv → CTRON_SEED 上位 → 宿主退役;
  固定点固化 ladder 第 7 步;原生 cc 扫全 suite_run/ev2 面;全语言发射域(struct/闭包/
  并发/插值/值位 if·match)按"编译任意 Ctron 程序"口径排期。
---
## 更新记录(21z session,C9j⑥ CLI 化 + 宿主上位)
- **C9j⑥ ✅**:发射器 CLI 化(零语言级全局):ct_find_main_anchor 探测输入 main 首个
  read_file 字面量 → ct_swap_anchor 纯 List 重建换 CTRONCLIINPUT 标记 → read_file 发射
  遇标记走 ctron_read_file_cli(ctron_anchor),驱动升 main(argc,argv) 支持 `run <file>`。
  产物即通用二进制:解释器形态 = 原生 cc,编译器形态 = 原生 ctron→C。
  ladder 升 **23 步 8 级全绿(--full)**:第 7 步宿主上位(原生 cc CLI 跑全部黄金+负例);
  第 8 步固定点(自编译逐字节复现 + CLI 编译 trans_v3 == 黄金)。旧第 6 步 sed 换锚退役。
  make test 全绿;trans_v0–v3 零回归。
- **钉子**:①改计划文档用 Edit 时 old_string 选"## 自举产物目录"会吞标题(三次)——
  追加条目应以末条正文为锚,并立即回查标题;②CLI 锚改写走"探测→换标记→发射遇标记
  换 ctron_anchor"三步,勿用语言级全局(发射器自身就得支持 static);③嵌套 run 的 rc
  语义:外层 cc main 恒 rc0(打印内层输出),负例拦截只看输出 grep——与种子逐层一致。
- 收官后格局:C 宿主只剩首次引导;剩余为可选项(两级引导脚本/全 suite_run 面/全语言
  发射域/ASan 迁移)。
---
## 更新记录(22a session,C9j⑦ 两级引导 + 全模块面双种子差分)
- **C9j⑦ ✅**:①`bootstrap.sh` 两级引导(阶段 0 C 宿主建 nc;阶段 1 CTRON_SEED=nc +
  CTRON_BOOT=1 重跑全阶梯)——日常验收自此可完全跑在自举二进制上。②ladder 第 9 步:
  16 模块面双种子差分,参考端恒定 C 宿主;新增 know- 三分口径(ok/known-div/fail)。
  ③cc.ct 补 read_dir 求值特判 + 发射器 ctron_read_dir/dirent.h/typeof。
  实测:bootstrap --full 35/0/4(4 个已知分歧如实标注);普通 ladder 38/0/0;
  **--full 负载原生比种子快 2.3 倍(136s→58s)**;make test 全绿。
- **双种子 sweep 的四个发现(全部已记账)**:①cc 求值器无 Float 域(trans_v2 原生种子
  `expr:Float`)→ 归"求值器运行域补尾";②自译化原生形态资源边界(cc×cc 嵌套 rc=137@63s,
  原生 malloc 不归还)→ 归内存管理;③④**C9i① 解析嵌套挂账实锤**:parsetree/sem_chk 的
  p_block 尾 guard,cc 解析器嵌在 while 外、rt 在循环内 → 原生 unbound:p0。
- **裁定留痕(重要,勿重蹈)**:排查 unbound:p0 时曾误删 run_block 的 env_drop(以为 rt
  是平铺 env)→ suite_diff input_cc 遮蔽用例立刻红(while 块外读在 rt 实为 abort,
  探针实证 rt 是标准块作用域 push/pop)。**已回退**。教训:动作用域语义前先用最小探针
  实证宿主行为,再对照 230 差分;bootstrap 的 sweep 是发现 cc-vs-rt 分叉的最强探针。
- 下一批候选:cc 求值器 Float 域;解析嵌套挂账修复(静机器+探针,见 21z 前记录);
  原生形态内存归还;发射器全语言域(struct/闭包/并发/插值/值位 if·match)。
---
## 更新记录(22b session,C9j⑧ cc 求值器 Float 域)
- **C9j⑧ ✅**:cc 求值器新增 ["D", 规范十进制文本] 值域(十进制定点:df_* 15 个 fn,
  I64 尾数 + scale,工作精度 9 位;df_g 实现 C %g 6 位有效 + e±XX 科学计数);
  fmt/truth/veq/vcmp/val_arith/Unary-Neg 全接入;发射器连带 **I64 支持**(类型码 "6" →
  int64_t,ctron_i64_to_string/ctron_print_i64,println/print/to_string 分发)。
  实测:浮点探针(整值 %.1f/0.1+0.2/1÷3 %g/1e-05 科学/负值/比较)三方逐字一致
  (rt == cc 求值器 == 原生发射);**bootstrap known-div 4→3**(trans_v2 转正);
  bootstrap 36/0/3、普通 ladder 38/0/0、make test 全绿(suite_diff 已至 238 cases,
  并行泳道增量)。cc decls 159→174(ladder check_decl 已同步)。
- **语言级发现(重要挂账)**:PascalCase 绑定名是解析歧义源——`var Qq = 7` 绑定名被
  p_pattern 按变体模式收(PatAgg-SubUnit),rt 绑定/读路径行为不一致(探针:同名小写
  全绿,大写绑定即未解析);cc 自身新代码 `var X` 即踩中(已改 xe)。规范应禁或
  解析/rt/生成码三处统一——归语言语义工作。另 df_* 自身限制挂账:二进制 double
  舍入边界/f32 后缀/浮点 to_string/div-by-zero inf(语料未触达)。
- 流程钉子:Ctron 无 `||`(又踩一次,df_g 初版)——写长表达式先想 or2;
  genmod 探针一律绝对路径;并行泳道活跃期(compiler_c parser_expr.c 在飞)提交前
  git status 逐文件核对。
---
## 更新记录(22c session,C9j⑨ 双种子差分清零)
- **C9j⑨ ✅ parsetree/sem_chk 转正(known-div 3→1)**:机械化括号映射 + 双解析器树
  diff 实锤 **cc 解析树与宿主树全一致**("解析嵌套挂账"猜想证伪)。真因 = 求值器作用域:
  rt 帧泄漏怪癖(env_pop 只移指针,调用密集路径旧帧可达)可观察地容忍 while 外读
  循环内绑定;cc 的 env_drop 即 unbound。修复:run_block keep 参数(While 体绑定留存,
  其余块照丢——input_cc 遮蔽 238 diff 守护)+ env_dedupe/While 轮末压缩(cc 的 env_add
  全量拷贝,keep 不压缩即 O(n²):sem_chk 计数 291s 被 jetsam 杀 → 修后 3s)+
  发射器 hoist 保守化(ct_assigned 递归零赋值才提升;For 子节点误取 st[len-2] 自伤已修;
  hreg 函数级登记防兄弟 while 重定义;ct_hoists 顺序穿 ew)。ct_stmt/ct_block/ct_if_stmt/
  ct_match_value 签名均 +hreg。
- 实测:bootstrap --full 38/0/1(唯一 known-div = 自译化原生形态资源边界);
  普通 ladder --full 39/0/0;make test 12 套件零失败;cc decls 174→175(env_dedupe)。
- **语言级发现(挂账)**:PascalCase 绑定名解析歧义——`var Qq = 7` 绑定名被 p_pattern
  按变体模式收,rt 绑定/读路径不一致(探针:同名小写全绿)。cc 自身 `var X` 踩中已改。
  规范应禁或解析/rt/生成码三处统一。
- 流程钉子:①插桩定位时 println 目标区分 stdout(发射产物)与 stderr(诊断),别混流
  grep;②zsh 管道 `cmd | tail; echo $?` 拿到的是 tail 的 rc —— 真实 rc 用无管道直跑;
  ③并行泳道活跃期(compiler_c parser_expr.c / suite_lex 63 文件在飞),只 add 自己的文件。
- 下一批候选:自译化原生形态内存归还(最后 known-div);发射器全语言域;CTRON_SEED
  默认切自举产物;PascalCase 绑定名禁用或统一(语言语义)。
---
## 更新记录(22d session,C9j⑩ arena 内存 + 测试提醒节点)
- **C9j⑩ ✅ known-div 清零**:发射产物运行时全面切换 **ctron_amalloc 凸分配 arena**
  (16 字节对齐,4MB 起步块倍增,旧块留存;str_concat/to_string/byte_slice/read_file/
  list_new/push 拷贝式倍增/cell_new 全走 arena;read_dir 保留 malloc 低频)。原生自译化
  cc×cc 此前 rc=137@63s 被杀 → **38s 完成、输出与黄金逐字一致**(较种子 127s 快 3.3 倍)。
  ladder step 5 解除 bootstrap 跳过(两种子都实跑)。**bootstrap --full 39/0/0
  known-div=0;普通 ladder --full 39/0/0;make test 12 套件零失败。**
- **⚠️ 用户提醒请求(必须履行)**:用户要求"自举版本能做测试时提醒我"——本节点已达成
  (bootstrap 全绿,自举二进制可跑完整验收阶梯),已在 session 结束时向用户正式提醒。
  后续:当自举版本具备更强测试能力(test 块批量运行/ctron test 等)时再次提醒。
- 发射器钉子:**Ctron 字符串转义只有 \{ 合法,\} 非法(E1001)**——闭括号写裸 };
  perl 批量替换后必须重扫转义与残留片段(list_new 曾留重复 magic 赋值、push 曾双倍扩容)。
- 下一批候选:发射器全语言域(struct/闭包/并发/插值/值位 if·match);CTRON_SEED 默认
  切自举产物;PascalCase 绑定名禁用或统一(语言语义);迁移 ASan 基建。
---
## 更新记录(22e session,C9j⑪ 性能基准套件)
- **C9j⑪ ✅**:`selfhosted/bench.sh` + `bench/` 四夹具。三形态首份系统性报告
  (详见计划 22i):原生代码生成计算密集快 8~26 倍;原生自译化快种子 3.8 倍(37s vs 141s);
  前端 parse+sem 持平(0.98);发射慢 1.66 倍(求值密集,tag 分发 vs 枚举分发);
  cc 解释器紧循环慢 rt 6 倍(env 持久 List 全量拷贝 —— 求值器优化点:结构共享)。
- 钉子:①macOS 新编译二进制首次执行有签名校验(~0.2-0.4s)—— 基准必须预热后取最小;
  ②perf 计时用 python 子进程内计时(perl/bash 启动开销污染亚秒测量);③zsh 管道吞 rc。
