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
compiler/ctc.sh check compiler/build/cc_run.ct   # decls=243 锁
```

**基线(2026-09-10,Apple Silicon)**:smoke --full 59/59(3b 夹具 + 2c 负例含递归泛型 emit 面拦截 + 3e 示例应用);
suite 51/51 双侧;decls=244;自举固定点(seed 发射 vs native 发射)逐字节复现;
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
- **Ctron 无 `continue`/`break`**:循环退出用标志位;无 `;` 分隔;无多返回值
  (用 List 或 env 变量)。
- **发射产物给 cc 必须以 `.c` 结尾**:`.ct`/`.em` → ld "unknown file type"。
- **改 src 后 bin/ 是旧的**:`native.sh` 不跑,一切 native 测试都在测旧代码。
- **timeit/重定向双 open 互踩**:捕获文件与产物文件不得同路径。
- **decls 锁**:smoke 锁 `decls=243`(cc_run 顶层 decl 数),加 fn/Static 须同步。
- **sem 遍历器下钻清单**:新增块类节点(如 Own)须在 tcb(sem_type)、ucb
  (sem_calls)、al_b、cscan_b 各遍历器补下钻,否则 E2020/E3070 误报/漏报。
- **打包同路径双写**:run_timed 捕获文件与产物文件同路径会互踩截断。
- **插值内字符串字面量 segfault**:`{"ab"}`(字符串字面量作插值片段)令解释器
  段错误(2026-09-10 发现,未修);`{7}`/`{true}`/`{struct 字段}` 正常。
- **bench.sh S4 的 CWD 依赖**:以仓库根为 cwd 时 seed 面锚 `../selfhosted/`
  解析到仓外 → "双形态输出分歧"误报(信息面不计门禁;实际两路输出一致)。
- **decls 锁现为 250**(CORE 侧 fmt_struct/eq_val + bound 检查 5 fn;std 泛型化/web 档的新 fn 全在 TRANS 不入 cc_run 锁);smoke 3b 含 derive、2c 含 fx_bound_neg + 递归泛型 emit 面拦截、3f 为 web 档三面。

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
5. **R-P2 对应**:std 容器泛型化(待泛型 struct 覆盖)、CI 例行化到远端。

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
- **标准库线:已完成 7eee4be**。stdpkg 现为 Map[K,V]/Set[V](双 List 平行槽,
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
  出 C stub。剩余:LSP(lsp/)、性能、`?` 传播 Option[Str] NULL 模型、
  `#[trusted]` 语义化、CI 远端例行化。
