# 自举编译器(compiler/)会话交接 —— 2026-09-10

> 交接自"分模块 → Phase 4 → Phase 5 → 泛型单态化"连续推进会话。
> 本文供新会话续接:现状 / 架构不变量 / 已知坑 / 挂账 / 续接入口。
> 上游:docs/superpowers/specs/2026-09-04-ctron-language-design.md、
> docs/superpowers/plans/2026-09-08-spec-gap-closure.md(Phase 0–5 全部收口)。

## 1. 一句话现状

`compiler/` 自举编译器(Ctron 写、36 个单职责模块)已完成 spec-gap-closure
Phase 0–5 全部主干:类型检查 v1、comptime 步数预算、多文件包、**真并发运行时
(pthread)**、`--profile` 档位、`\u{HEX}`、发射器全语言覆盖(struct/用户枚举/
一等 fn 值与带捕获闭包/typed List/own/`?` 传播/**泛型 fn 与 struct 单态化**)。
CI 门禁(ci.sh + Actions workflow)与性能基线(bench.sh)入册。
全部在 main,工作树干净。

## 2. 验证口径(改完代码必跑)

```sh
make -C compiler-c                    # 宿主 seed(仅首次引导;已构建可跳)
compiler/build.sh                     # 拼接三产物 cc_run/cc_check/cc_emit
compiler/test/smoke.sh --full         # 54 项(发射面夹具 seed==native 逐字 + 固定点)
compiler/native.sh                    # 重建 bin/ctron-cc / bin/ctron-emit ← 改源后必跑!
python3 compiler/test/suite.py        # tests/ 一致性 51/51 对照 C 参考宿主
./ci.sh                               # 一条命令全量(meta/拼接/smoke/native/suite/bench)
compiler/ctc.sh check compiler/build/cc_run.ct   # decls=271 锁
```

**基线(2026-09-12,Apple Silicon)**:smoke --full 88/88(3b 夹具含 optstr/gprobe2/gprobe/fmap/fs 探针
+ 3d std 包泛型容器 + 3d- use 撞名 E5030 + 3e ctwc + 3f web 档 + 3g ctwf/fmap/sort/map/set/fs 单测 + 2c 负例含 fx_litfit_neg E2040 + 递归泛型 emit 面拦截);
suite 58/58 双侧;decls=268;自举固定点(seed 发射 vs native 发射)逐字节复现;
native 发射 cc_run 0.08s vs seed ~13s(行数随 TRANS 演进,ci 实测为准);代码生成比解释快 15–100×
(bench.sh 四阶段,基线表见 BOOTSTRAP.md §2b)。

## 3. 架构不变量(新会话必读)

1. **模块化 = 确定性拼接**:Ctron 单文件程序模型,`build.sh` 按 CORE(lex +
   parse_\*5 + sem_\*14 + eval_\*9)与 TRANS(trans_ty/expr/stmt/conc/emit)两组
   **保序**拼接;trans 仅入 cc_emit。改代码 = 改 src/*.ct 后跑 build.sh。
2. **双遍发射协议**(trans_conc.ct):`ct_fn(d, file, pass)` 以 env `"#p"`=1/2 跑
   两遍;pass1 的 eln 全门控(丢弃),仅 spawn shim / mcell typedef / 泛型特化
   定义直出(文件作用域件,先于函数体);pass2 正常发射。**为什么不静态缓冲**:
   seed 解释器 List 绑定是值语义(读即深克隆),跨 fn 可变通道不存在。
3. **泛型单态化**:显式 TypeArgs 调用点 → pass1 直出特化(AST 型别替换
   Named(TPar)→实参型节点;mangle `t_name__<码>`);返回码经 TPar 替换推导
   (F<ar>=返标量 / G<ar>=返 fn 值;E:<名>=用户枚举实例)。v0 限制见 §5。
4. **并发运行时**(driver_emit 样板 + trans_conc):ct_scope/ct_task/ct_chan/
   ct_res;任务 panic → longjmp → scope cancelled + **持通道锁广播**(丢失唤醒
   已修,1f6d05a)→ 阻塞 send/recv 得 Err("ScopeCancelled")。
5. **eval 语义锚**:发射与 seed 解释逐字对齐。List 绑定值语义(读即深克隆,
   仅 M=Atomic/Global 保身份);闭包捕获 = 创建时快照;send 满/recv 尽 →
   Err("ScopeCancelled");fetch_add 返回旧值;Mutex.with 值拷贝/with_mut
   指针可见写。

## 4. 已知坑(血泪清单,违反即翻车)

- **`&&`/`||` 不短路(eager)**:复合守卫 `a && b[1][0] == "X"` 在 a 假时仍
  求值 b[1] → 越界。一律拆嵌套 if。
- **字符串字面量**:裸 `{` 开启插值(未终止 → 解析死循环/误报),必须 `\{`;
  `\}` 是非法转义(裸 `}` 即可)。批量修复脚本模式见本会话(逐字符状态机扫描)。
- **eval 对 `*`/`+` 溢出有守卫(直接 panic)**:大数算术/哈希一律 mod 小素数
  (fmap 的 djb2 mod 100003 即此因);`+%`/`-%` 是回绕加/减,无回绕乘。
- **use 撞名现为 E5030 拦截**(feat/std-stdlib 起):同名 decl 曾"首个胜出"静默
  遮蔽,现为装载期错误;未来若需 shadowing 语义须显式设计(如 as 重命名导入)。
- **bound 传递显式化**:TPar→TPar 传参(如 fromlist[V] 调 add[V: Eq])要求外层
  fn 自带同名 bound,否则 E2050——容器便利 fn 签名须与被调 bound 对齐。
- **Str 接收者的 .to_string()**:eval 恒等(vS(fmt));emit 原缺 "s"/"N" 分支
  (曾掉 i32 兜底打印指针低位)——已补恒等发射。
- **多文件包调试铁律**:bin/ 陈旧与 ctc.sh emit 无 path 回退会叠加出
  't_xxx undeclared/型别错乱'假象——排障前先 native.sh + bin/ctron-emit run。
- **泛型 struct 构造点具体 typed List 字段:已解(feat/std-stdlib)**——
  ct_structlit_inst 槽对齐('#<名>' env 钉定优先;场值码经"声明字段 → 型参槽"
  统一:裸 TPar 直取值码,List[TPar] 值码 LI→元素 i/其余→元素 s),typeof(StructLit)
  同源取码;fx_gprobe 回归夹具锁定。已知损失:List[TPar] 槽值码非 LI 一律归
  元素 s(List[Bool]/List[struct] 字段与 List[Str] 同槽码,v0 文档化)。
- **fs 内建已入前奏(feat/std-stdlib)**:fs_exists/fs_write/fs_delete(Bool 域)
  四层对齐:宿主 rt_eval + cc eval_call + trans 分发 + driver_emit 运行时;
  sem 前奏两处名单已同步。新增内建的完整清单(五处:前奏两处/eval/分发/运行时)
  即新增内建 SOP。**now_ms 已落地(feat/std-time)**:now_ms() -> F64(D 域)+
  伴生 now_ms_text()(规范十进制文本→vD 包装;cc eval 无法在 Ctron 侧格式化
  宿主 double,故双内建)。**新教训**:新增内建第六处——ct_typeof 内建返回型别表
  (漏配 now_ms 时局部按 i32 推,double 截断溢出,fx_time 首跑踩中)。
  **I64 值域已落地(feat/i64-arith)**:["6", 规范十进制文本],v6/c6can/c6cmp
  /c6add/c6sub 之外补 c6mul(竖式)+c6divmod(长除试商,"商|余数"复合串)+
  val_arith Mul/Div/Mod 分支(除零 panic 镜像 I32)。**负数 Div/Mod 已实现
  C99 截断**(d2f6b1d 即含:商向零取整、商符号异或、余数符号随被除数;
  2026-09-12 运行时验证 -7/2=-3、-7%2=-1、7%-2=1、-7%-2=-1、0/x 与
  Mul 符号全对)——旧记"负数 v0 入 panic"系 commit message 与行内注释笔误,
  本轮已纠正(注释 + 兜底消息"仅 Add/Sub v0"→"不支持的算符")。
  **值模型精确口径**:seed 侧规范十进制文本(c6can 去前导零、-0 归 0),
  发射侧 int64_t(trans_ty 码表);**算术无溢出检查**(文本域自然不溢出,
  发射侧未查)→ 超 int64 宽度双实现分歧,v0 夹具限宽度内。**宽字面量直入
  6 域已落地**:纯十进制无后缀字面量超过 I32 宽度时 eval 取 v6(c6can(数字)),
  发射侧加 LL 后缀 + ct_typeof 推断 "6"(下划线分隔符剥离);**E2040 字面量
  宽度门**(let 注解/赋值/返回/实参四点;0x/0o/0b、comptime 域、二进制折叠
  表达式、带后缀字面量 v0 不查);**conv_as 补 6 域入口**(conv_as_6 二补
  截断:8/16 位落 W、32 位落 I、64 位留 6 域;as[U32] 不截断曾致 suite 03b
  分歧,已修);fx_litfit_neg 负例。
  以上已成文 spec §3.1.1/§10。to_string/fmt 走十进制
  文本恒等。decls 锁 269。
- **闭包捕获 typed List(LI):已修(feat/std-time)**——真凶不是捕获码计算,
  而是 ct_cap_decl/ct_wrap_i 的指针域白名单缺 "LI"(typed List 码),decl 侧
  掉 (int32_t) 截断。已并入 L 分支。残余限制:**闭包(ct_clop)形参 ABI 为
  32 位 ct_i**——Str/List 实参经形参传递仍是截断 UB(排序谓词请用 I32 索引
  闭包 + 捕获句柄的口径,ctwf Top-3 即此);32→64 位 ABI 升级挂账。
  **排障教训**:'捕获坏了'一度是 vendored 副本漂移 + 陈旧 bin 的叠加假象,
  最小包先复现再下结论。
- **闭包形参 32 位限制的 ABI 调研结论(feat/closure-abi)**:ct_i 实为 int64
  (emitted `typedef int64_t ct_i`),传输层 64 位无恙;截断仅发生在 trans_conc
  闭包 shim 的形参声明硬编码 `int32_t t_x = (int32_t)_pN` + env 绑定 "i"。
  升级方案草案:TypeArgs 调用点把目标 fn 型形参的替换后内参码经 env
  '#clcodes'(逗号串)提示传入 shim,按码生成形参声明。
  **已知阻塞(feat/closure-abi 实测)**:该提示块进 trans_expr 后,
  cc_emit 自发射在 Let 型别推断路径触发 "byte_at 目标需 Str"(与块内逻辑
  是否执行无关,疑似与 pass1 遍历/ps2 作用域交互)——根因未钉死前,
  A 块保持移除;Str/List 过闭包形参继续用 I32 索引闭包 + 捕获句柄口径
  (ct_ctype + (long) 中转)。**止损原因**:提示需穿过预提升/#spec 多层 env,
  叠加自发射回归排查成本超出单片边界;正式升级建议:shim 形参码化 +
  spawn shim 同步 + ct_cap_decl/ct_call_args 全链审计,由熟悉闭包机制的
  lane 主导。**wip 快照真相(2026-09-12 核实)**:0291aa2"闭包 lane 在途工作
  (+516 行)"经逐行比对为**纯空行插入**(非空行集合与父提交 c685df7 逐行
  相同,509 行),无任何实质闭包代码落盘——勿据该提交续推;当时门禁绿系
  空行不改变行为。在途工作若存在,须由闭包 lane 重新落盘。
  另:Ctron 逻辑或是关键字 or(p_or 匹配 'or' 记号),
  '||' 非语法——新代码一律 or2/or3。
- **`continue`/`break` 已自举落地(v0.7 修订二,9d56b73:parse/eval 状态种 b·c/trans 直映/sem E2070;E2071/E2072 门待补)**:旧语料循环退出仍用标志位;无 `;` 分隔;无多返回值
  (用 List 或 env 变量)。
- **发射产物给 cc 必须以 `.c` 结尾**:`.ct`/`.em` → ld "unknown file type"。
- **改 src 后 bin/ 是旧的**:`native.sh` 不跑,一切 native 测试都在测旧代码。
- **timeit/重定向双 open 互踩**:捕获文件与产物文件不得同路径。
- **decls 锁**:smoke 锁 `decls=271`(cc_run 顶层 decl 数),加 fn/Static 须同步。
- **sem 遍历器下钻清单**:新增块类节点(如 Own)须在 tcb(sem_type)、ucb
  (sem_calls)、al_b、cscan_b 各遍历器补下钻,否则 E2020/E3070 误报/漏报。
- **打包同路径双写**:run_timed 捕获文件与产物文件同路径会互踩截断。
- **插值内字符串字面量 segfault**:`{"ab"}`(字符串字面量作插值片段)令解释器
  段错误(2026-09-10 发现,未修);`{7}`/`{true}`/`{struct 字段}` 正常。
- **bench.sh S4 的 CWD 依赖**:以仓库根为 cwd 时 seed 面锚 `../selfhosted/`
  解析到仓外 → "双形态输出分歧"误报(信息面不计门禁;实际两路输出一致)。
- **decls 锁现为 271**(feat/i64-arith I64 域 + match 守卫 + 宽字面量切片
  lit_is_dec/lit_digits/lit_wide_i32/lit_fit_gate/lit_radix_dec/lit_to_dec/conv_as_6;
  CORE 侧 fmt_struct/eq_val + bound 检查 5 fn;std 泛型化/web 档的新 fn 全在 TRANS
  不入 cc_run 锁);smoke 3b 含 derive、2c 含 fx_bound_neg + fx_litfit_neg +
  递归泛型 emit 面拦截、3f 为 web 档三面。
- **含 E/e 的十六进制字面量误判 Float(已修,本会话词法切片)**:parse_expr
  数字 token 的浮点检测曾按 'e'/'E' 判 isf,0xDEADBEEF 类字面量被建成 Float
  节点 → E2010。现 0x 前缀整字面量不作浮点指数判定(isf 仅 '.'/非 hex 的
  e/E);fx_time 含 dead/cafe 双锚。

## 5. 挂账(按优先级,均为独立切片)

1. **泛型深水区**:泛型 struct 方法、泛型体内嵌泛型调用、嵌套泛型
   (`Fn` 返回 `Fn`)、bound/derive 体系(**@derive(Show) 55adfae、
   @derive(Eq) 402a65c 已落地**:eval/emit 双面逐字对齐,fx_derive 夹具;
   派生为结构化——字段全可显示/可比较即有 .show()/.eq(),注解仍声明性;
   print(Str) 缺括号)。**bound 强制检查已落地 f3ef54a**:E2050 于显式 TypeArgs
   调用点核对 TPar bounds(sem_type.bound_sat 结构化谓词,与派生能力面一致;
   fx_bound_neg 负例;README 码表已登记)。仍挂账:泛型 struct 注解实例化点的
   bound 核对(现为 fn 调用点 only)、嵌套泛型 TPar 实参的传递核对(v0 放行)、
   @derive(Json) 等更多插件。单态化机制已备好
   (AST 替换 + pass1 直出 + 形参/型参/'#实例' env 绑定 + 嵌套预提升 #ph/#spec),
   扩展点在 trans_expr TypeArgs 尾部与 ct_mono_subst_ty。多文件包发射必须用
   bin/ctron-emit(ctc.sh emit 无 path 回退,driver_emit 注释已文档化)。
   **改 trans 源后测 bin 路径前必跑 native.sh**(本轮三次踩陈旧二进制)。
2. **arena API 发射**:own 块已透明发射(41b7751),但 `arena.array[T](n)` /
   `.push` / `.into_gc` 等方法仍 panic(05_own 语料原生不可跑)。
3. **`?` 传播 Option[Str] NULL 模型**、**装箱载荷(用户枚举入 Result)别名
   语义细化**、**`#[trusted]` 语义化**(现为解析兼容 + FFI 信任占位)。
4. **--profile web 语义化**(现为 full 别名;发射 C11 可走 Emscripten/wasm32)。
5. **R-P2 对应**:std 容器泛型化已落地(7eee4be);**feat/std-stdlib 续片**:
   `std/fmap.ct`(FMap[V] Str 键哈希映射:djb2 mod 100003,256 槽开放寻址,
   slots 存索引规避泛型零值,函数式 fput,迭代序=插入序,f 前缀命名避撞名)、
   `std/sort.ct`(sorted[K]/sorted_desc[K] 泛型稳定插入排序 + reversed[K])、
   `std/str.ct` 扩面(join/starts_with/ends_with/trim + 单测)、
   **examples/ctwf 词频统计示例**(R-P2b 出口兑现:词频对 sort/uniq 黄金,
   汇总行 distinct/total,双跑确定性)。随片发射修复:**Str 关系运算 strcmp 化**
   (Lt/Gt/Le/Ge——裸 `<` 曾是指针比较)、**LI 索引写装箱**((const char*)(long)
   镜像读侧)、**实例化 typedef 预扫卫兵**(CT_TDEF_ 与特化点同宏互斥)。
   **迭代面已收口**:map keys[K,V]/values[K,V]、set tolist[V]/fromlist[V: Eq]、
   fmap fvals[V]、std/fs.ct(read_or/exists 便利层,+单测);stdpkg main 全覆盖。
   **时间原语已落地**:now_ms() -> F64(五层 SOP+typeof 型别表)。
   **谓词排序已落地**:sorted_by[K]/sorted_by_desc[K](less: fn(K,K)->Bool,
   稳定插入排序;fn 类型参数经单态化 + ct_clop 间接调用;闭包形参限 I32/Bool,
   Str 经形参为截断 UB——用 I32 索引闭包 + 捕获句柄口径)。ctwf Top-3 实战
   (索引比较器 + 捕获三个 List 句柄,smoke 3g 专项断言;3h vendored 同步检查)。
   **键排序已落地**:sorted_by_keys[V](xs, keys: List[Str], desc)——I32 索引
   比较 + Str 键字典序(调用方保证键宽度一致:数字补零/ISO 时间),完全避开
   闭包形参 ABI。
   **余项精确口径(2026-09-12 入册)**:
   ① **闭包 ABI 32→64 位升级**——§4 草案与阻塞点(A 块自发射回归)仍有效;
      0291aa2 快照纯空行无实质代码,续推须 lane 重新落盘(§4)。
   ② **I64 用户级值域专片已落地**(d2f6b1d:Add/Sub/Mul/Div/Mod 全算符,
      负数 Div/Mod C99 截断已实现并运行时验证,见 §4 值模型口径);**宽字面量
      直入 6 域 + E2040 宽度门 + conv_as_6 截断已落地**(见 §4);残余:
      0x/0o/0b 宽字面量与 comptime 域不进门(v0 口径)、发射侧溢出未检查/
      超 int64 宽度双实现分歧收敛。
   ③ **spec 文档化已落地**(本轮):docs/spec/03-types §3.1.1(I64 值域 v0
      实现口径)+ README 修订记录"登记"条。
   ④ CI 例行化到远端。

## 6. 关键文件地图

| 文件 | 内容 |
|---|---|
| `compiler/src/trans_conc.ct` | 并发/闭包/双遍协议(eln)/spawn/with/parallel |
| `compiler/src/trans_expr.ct` | 表达式发射(TypeArgs 泛型/构造器/成员分派/Own) |
| `compiler/src/trans_stmt.ct` | 语句发射(eln 化;match R/E 臂;字段赋值;Try) |
| `compiler/src/trans_ty.ct` | 类型码全表(i/s/b/f/L/LI/A/N/6/g/k/h/R/m/P/u:/E:/F/G)+ 泛型助手 |
| `compiler/src/driver_emit.ct` | 并发运行时样板 + Static 发射 + 双遍循环 + typedef 预扫 |
| `compiler/src/parse_pkg.ct` | 模块加载器(std.* → 入口旁 std/;E5020/E2020/caps) |
| `compiler/test/fx_*.ct` | 17 个发射面夹具(fx_conc_\*7 + fx_tlist/generic/gstruct/fnval/cloval/enumres/own/try/bare_neg/uhex) |
| `compiler/test/stdpkg/` | std 种子(std/map.ct IntMap、std/set.ct IntSet,函数式) |
| `docs/superpowers/specs/2026-09-09-compiler-src-module-split-design.md` | 模块树设计 |
| `docs/superpowers/plans/2026-09-10-phase4-concurrency-runtime.md` | Phase 4 计划(已勾账) |
| `docs/superpowers/plans/2026-09-08-spec-gap-closure.md` | Phase 0–5 总计划(状态已同步) |

## 7. 续接入口建议

- **继续发射器深水区**:先读 §3/§4,从 bound/derive 体系切入(泛型语料
  03e 的唯一宿主依赖)。
- **标准库线:7eee4be + feat/std-stdlib(fmap/sort/str 扩面/ctwf/E5030)**。stdpkg 现为 Map[K,V]/Set[V](双 List 平行槽,
  函数式 API,K/V 需 Eq bound;调用点显式 TypeArgs 实例化)。顺带补全发射面
  单态化深水区:subst 嵌套下钻、List[K] 字段/构造的元素码、StructLit '#实例'
  env 绑定、#ifndef 特化去重、typedef 直出前置。**泛型体内嵌泛型调用已落地
  fb65b2b**:ct_targ_env 型参实参 env 解析 + 嵌套单态化预提升(#ph 静默遍历
  直出定义/#spec 体只发调用),set.add 已恢复嵌套调 mem[V],fx_generic 有
  嵌套链用例。**递归泛型防护 + TPar bound 传递已收口 af6fd95**(预提升深度门限 3 超限
  panic '递归超限';bound_sat 顶层无声明名视为外层型参放行,原语/枚举/类仍拦)。
  泛型深水区至此全部收口。
- **规范 v0.6 已成文(2026-09-11)**:`docs/spec/` README bump v0.6 +
  03-types §3.9/§3.11(泛型/derive 语义成文)+ §10 补 E2050 + tests/README
  钉子 19–21。**`--profile web` 已语义化 3cf2999f→3cf299f**:stdweb.dom 最小 API
  检查/运行面可用(10_web_dom.ct web 档转绿;full 档 E2020 拦截),发射面
  出 C stub。**`?` 传播 Option[Str] NULL 模型已落地 635d7f4**(表达式位 Try/let 位载荷泛化/
  Option 臂匹配/veq·fmt 补 T;fx_optstr 双面逐字)。**`#[trusted]` 已语义化 2aaaee3**:extern 缺标记 W8050/trusted 放非 extern
  E4040/check --trusted 审计枚举(注册表已入 spec §10 与 README §4)。
  **装箱载荷别名语义已细化 5e007e5/e8ea6e9**:Box 赋值/传参共享堆 cell,
经别名的可变字段写全可见(自动解引用贯通读写;用户声明 Box struct 时内建
让位通用泛型路径);fx_boxalias 双面逐字。剩余:LSP(lsp/)、性能、
CI 远端例行化。**I64 域残项(2026-09-12 入册,宽字面量切片后更新)**:0x/0o/0b 宽字面量
与 comptime 域入门、发射侧溢出检查与超 int64 宽度双实现分歧收敛——spec
§3.1.1/§10 已按 v0 实现口径成文,升级时同步改。
