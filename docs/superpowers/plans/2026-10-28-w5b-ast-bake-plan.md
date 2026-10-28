# W5b 编译期烘焙批:entry AST 缓存(.cache/ast)计划

> 创建:2026-10-28。上游:COVERAGE.md:3554-3565(W5b 立项:解析税 60–80% 实测,
> 触发条件达标转正式候选批);底座 = .ctast S1a-i/ii/iii(driver_ast.ct,100 标签,
> roundtrip 固定点全绿);S1b 缓存接线既定挂账(specs/2026-09-21-closed-pkg-distribution-design.md:304-305,
> 当年占比 ≈44% 未触发,今触发)。注意与 TUI 域 W5b(ctml 烘焙,2026-10-09-tui-design.md:231)同名异批。

## Goal
编译链 parse 税消除:seed-interp 链上同源重复编译命中 `.cache/ast/` 免 parse
(加载 .ctast 直入语义/解释/发射),目标编译链 2–3× 加速(基线 2m32.6s parse-only /
2m0.6s 全链,COVERAGE.md:3557-3560);正确性门全绿零扩大。

## 裁决实录(开工前拍板)
- **U1 测量输入**:「24K 行」= cc_run.ct 编译链本体(读法 A);开工第一步以
  `ctc.sh ast` 复测基线定谳,收益声明按复测值。
- **U2 load 代价**:新增 `--ast=loadcheck` 模式(load→re-emit 字节自比)先测,
  2–3× 承诺前必须过。
- **U3 O(n²) join**:driver_ast.ct:150-157 逐块串联,MB 级下吃收益——本批先测
  (loadcheck 计时内含),恶化明显再做分块写。
- **U5 cols/lns**:行号戳为节点 S 槽随树保存(roundtrip 已证无损);cols/pdiags
  不随树,命中路径按构造为空(键含源哈希 → 源变即 miss → 全诊断重取);fmt 臂
  不接缓存(token 流,无 AST 消费)。
- **U6 非 std 依赖**:v1 只做入口文件缓存;use 依赖闭包缓存(v2)待用户项目
  需求方裁决。
- **U7 同名异批**:本批登记/汇报冠以「编译器 parse 缓存 W5b」。
- **信任级**:封印工件 fail-closed(E5054/E5055)不变;开发缓存 = 不可信加速物,
  **坏缓存回退 parse 而非中止**——壳层侧 `<key>.sha` 副签校验(摘要计算在编排层,
  D8-2 冻结口径,驱动内禁 sha:seed-interp sha256_hex 为 356s 慢径)。

## 设计
- **键**:`ctc_sha(源) + "_" + sha12(cc_<mode>.ct)`(T20 内容寻址 emit 缓存同构,
  ctc.sh:406-418 先例;编译器版本经 ANCHORVERSION 嵌入 cc_*.ct 被自动覆盖;
  run/check 的 doctest_lift 不对称由驱动身份入键天然消解)。
- **布局**:`$ROOT/.cache/ast/<key>.ctast`(格式 v1 字节流,与 `--ast=dump` 同构)
  + `<key>.sha`(ctast 副签)。
- **命中路径**:ctc.sh run/check/emit 臂算键 → ctast+sha 齐且 shasum 对 →
  `export CTRON_AST_IN=<cached>`;不齐/坏 → `rm` 双件 → `export CTRON_AST_FILL=<cached>`。
- **驱动侧**(driver_run.ct:44-51 / driver_check.ct:260-263 / driver_emit.ct:17-20
  三入口):`CTRON_AST_IN` 非空 → read_file 缓存 → `N ` 前缀预检 → ast_load_node
  直取 AST(跳过 lift/scan4/p_file,pdiags 按构造为空);`CTRON_AST_FILL` 非空 →
  parse 后 ast_emit_node 序列化回填(E-AST-UNKNOWN/-SHAPE = 不写缓存,编译照常,
  覆盖缺口只损失命中率);两 env 皆空 → 全 parse 现状。
- **build 臂搭车**:ctc.sh:485 `$0 emit` 子壳继承 env,零改动。
- **失效**:纯内容寻址;无 GC(v0);shasum 缺席 = 缓存禁用(T20 同款)。

## Tasks
1. 驱动侧三入口命中/回填钩子(读 driver 入口实码后落位;ast_load_node/ast_emit_node
   已在 CORE,零拼接变更)。
2. ctc.sh 三臂键算 + 副签校验 + env 导出 + `.cache/ast/` mkdir。
3. smoke 新腿:warm/cold 对拍(emit 冷热产物 C 逐字节同、check 冷热诊断同、
   run 输出同)+ 腐坏演练(截断/篡改 ctast → 副签不匹配回退 parse,输出与冷径一致)。
4. `--ast=loadcheck` 模式 + 解析税四元组计时(ast/emit × 冷/热),输入 = cc_run.ct
   本体 + 小夹具基线;2–3× 达成判定。
5. (v2,不在本批)use 依赖闭包缓存:parse_pkg.ct:935-947 `.ast` 分支旁缓存分支 +
   std.dig。

## DoD
- smoke --full 全绿(现 246/0 基线 + 新腿);suite.py 与基线指纹一致零扩大。
- 热路径对拍:编译产物/诊断/运行输出冷热逐字节一致;计时收益 ≥2×(cc_run 链)。
- roundtrip 现役腿(geom/stdpkg/fx_use_alias,smoke.sh:801-817)保持绿。
- COVERAGE 批登记 + 本计划 as-built 回填。

## Self-Review
- 哈希全在壳层;驱动零 sha。
- 命中路径 pdiags 为空是**按构造**(键含源哈希),非假设。
- 序列化 fail-closed → 有损树进不了缓存,风险降级为命中率。

## As-built(2026-10-28,同日落库)
- **落地**:Tasks 1–3 全量(三驱动钩子/ctc.sh 键算+副签/腐坏回退/smoke 3r 腿);
  ast_emit_str/ast_emit_node 迁 CORE ast_fmt.ct(回填助手依赖);ast_read_line
  两通道化(逐字符拼接 O(len²) 修正);**默认关(opt-in CTRON_AST_CACHE=1)**。
- **U1 定谳**:「24K 行 cc_run 链」单文件输入在本机 seed-interp 下 run/emit 双
  OOM(rc=137,~2m18)——COVERAGE:3554 基线口径不可复现(输入未点名);
  实测改用 lib/std/json.ct(1980 行)。
- **U2 判定(核心结论)**:**load 代价 > parse,立项前提证伪**——json.ct
  cold(fill,含序列化)2.75s vs warm(AST hit+emit)3.89–4.10s;两通道修复前
  warm 4.5–5.6s。2–3× 判定=**不达成**。机制保留默认关,翻转条件 = host 内建
  C 载入或自举 bin 面(load<parse 交叉点,设计裁决另立)。
- **正确性全绿**:冷/热产物 C 逐字节一致;腐坏(截断)副签拒收回退 parse 输出
  不变;roundtrip 现役腿随 smoke 复验。
