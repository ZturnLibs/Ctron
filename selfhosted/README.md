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
| `pkg_chk.ct` | 模块级检查 **逐字 oracle**:toml 解析(Ctron.toml)+ E5010/E2020/E4010/E5020(树形 DFS)+ **E6010 comptime 预算**(纯 int 求值器,steps/depth 超限;spin 命中) | 与 `ctronc pkg` 逐字一致(seq=8,7 包) |
| `ev_num.ct` | 执行种子:树上数值求值(17/256/-15/3) | C9b-0 |
| `ev2.ct` | **Ctron 求值器扩面**:纯函数式树行走解释器(Bool/Str/变量+块作用域/if/while/for/递归/test+assert/输出),主模式+测试模式自判 | C9b① seq5/6 与 C rt 逐字一致 |
| `ev2.ct`(续) | **运行域对齐**:match(字面量/通配/绑定)+ 数组(字面量/索引/.len/for-over-array) | C9d① seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **Option/Result 域**:tag 值 Some/None/Ok/Err(构造/裸 Ident)+ match 模式绑定 + `?` Try 传播 | C9d② seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **struct/枚举域**:StructLit/字段读写/struct 模式绑定 + 用户枚举 unit 变体(裸值/SubUnit) | C9d③ seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **UFCS**:`x.f(args)` → 文件自由 fn 首参调用 | C9e① seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **闭包/fn 值**:裸 fn 名 fn-ref、闭包字面量捕获、fn 类型参数高阶调用 | C9e② seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **List 域(引用语义)**:List[T]()/arena.list[T]() 构造、push 原地变、.len、索引读/写/复合写、别名可见、into_gc() 深拷贝隔离 | C9f① seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **trait/impl 方法域**:impl 方法分发、trait 默认方法体(空 impl)、impl prop、self 互调、类实例字面量 | C9f② seq=5 与 C rt 逐字一致 |
| `ev2.ct`(续) | **绑定克隆与原地写**:结构体绑定深克隆、类引用共享、self 可变方法、成员写原地透 | C9g seq=5 与 C rt 逐字一致 |
| `cc.ct` | **统一 cc 驱动**(自足快照 5.5k 行:parse→单文件语义 12 项→run 全量求值器):有诊断输出 `CODE: msg` 并止;干净则解释执行 main/test | C9h 刷新至 C9g,seq9 正/负夹具与 C 管线逐字一致 |
| `cc.sh` | **独立工具链驱动**:任意 .ct 输入 → cc.ct 编译运行(换靶模板锚 + 宿主 seed 解释) | C9h |
| 自编译检查 | cc 全管线 parse+sem 自身/互检源码全绿:input_cc3(10)/sem_chk(109)/parsetree(57)/**ev2(106)/**/**cc(158)**,decl 数与 C 解析器逐一吻合(≤4s) | C9i② |
| **全深度自译化** | cc.ct 解释 cc.ct(163KB)parse+sem+运行嵌套 main,235s rc=0,输出与直接管线逐字一致 | C9i② |
| 解析器韧性 | NL 换行过滤(§1.6 续行)+ p_file/p_block 停滞守卫(循环体内 ensure_progress)+ StructLit LitFs #EOF 守卫 + allow_struct 线程化(条件上下文禁结构体字面量) | C9i①② |
| 新增内建(求值器) | read_file(Some/None)/ Atomic 构造与 load/store(M 值共享单元)/ unesc(字符串转义展开,镜像 C 词法器) | C9i② |
| **两级引导** | `./bootstrap.sh [--full]`:阶段 0 C 宿主建第一个原生 cc(唯一宿主依赖),阶段 1 以自举产物为种子重跑全部阶梯(CTRON_BOOT=1,已知能力缺口 know- 标注) | C9j⑦ |
| **本地验收阶梯** | `./ladder.sh [--full]`:黄金对照+负例拦截+自编译阶梯+复现回归+代码生成往返+全深度自译化+自发射收官+**宿主上位+自举固定点+全模块面双种子差分**,一次跑完(38+ 步 9 级) | C9i③–C9j⑦ |
| **C 代码生成器** | `tools/trans_part.ct`(Ctron 写,v0→v3):数值/Str(strcmp·拼接·按类型分发)/定长数组·for-in/浮点·assert → **List[Str](引用语义)/Atomic[I32]/索引读写/节点引用 N/match-Option/read_file·byte_at·byte_slice·to_string/函数原型前置/尾值返回/N `.len` 魔数动态分派**;trans_v0–v3 往返逐字一致;**自发射收官:cc.ct 经其发射 9.9k 行 C → gcc → 原生自举 cc 解释 input_cc3 == 黄金逐字一致**(ladder 第 6 步);**自举固定点:编译器编译出的自身再编译自身 == 逐字节复现,且能编译用户程序 == 黄金** | C9j①–⑤ |
| 工具链文件 | tools/genmod.py(模块生成器)/ expected/(黄金基线)/ repro/(复现夹具) | C9i③ |
| 解析器韧性 | NL 换行过滤(§1.6 续行)+ p_file/p_block 停滞守卫 + StructLit LitFs #EOF 守卫(条件内误触发不再挂起) | C9i① |
| `input_*.ct` | 差分夹具(含 `input_ev2*.ct`、`input_cc.ct` 主程序、`input_cc_neg.ct` W8010 负例) | — |

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
- C9b②(Ctron 模块级 oracle + E6010)已交付:`pkg_chk.ct` 镜像 C pkg 全流程(归属/顺序/消息),
  并新增 comptime 预算求值器(无限递归 spin 命中 E6010);suite_diff seq=8 对全部 7 个模块包
  (orphan/circular/visibility/caps/comptime_budget + use_ok/ffi_math)与 `ctronc pkg` 逐字一致(218 cases);
- C9c(统一 cc 驱动)已交付:`cc.ct` = sem_chk(parser+语义 12 项)+ ev2(解释器)自足快照,
  parse→sem→run 单入口;有诊断输出并止(rc1),干净则解释运行(rc0);seq=9 正/负夹具与 C 管线逐字一致;
- C9d①(运行域对齐:match + 数组)已交付:`ev2.ct` 补 match(I32/Bool/Str 字面量/通配/绑定)
  与数组(字面量/索引/.len/for-over-array),input_ev2b.ct 与原生逐字一致(221 cases);
- C9d②(Option/Result 域)已交付:tag 值/变体构造/match 绑定/`?` Try 传播,input_ev2c.ct 与原生逐字一致(222 cases);
- C9d③(struct/枚举域)已交付:StructLit/字段读写/struct 模式/用户枚举 unit 变体,input_ev2d.ct 与原生逐字一致(223 cases);
- C9e①(cc 快照刷新 + UFCS)已交付:cc.ct 重拼至 C9d 求值器并新增富程序正例 input_cc2.ct;
  ev2.ct 补 UFCS(21.double()/6.scale(7)),input_ev2e.ct 与原生逐字一致(225 cases);
- C9e②(闭包/fn 值)已交付:裸 fn 名 fn-ref、闭包字面量捕获、fn 类型参数高阶调用(input_ev2f.ct,226 cases);
- C9f①(List 域引用语义)已交付:List[T]() 构造、push 原地变、.len、索引读/写/复合写、别名可见、
  into_gc() 深拷贝隔离(input_ev2g.ct,227 cases);已裁定钉子:for-over-list 宿主不支持(夹具避开)、
  println(List) → `<value>`、越界走宿主 panic 不入差分;
- C9f②(trait/impl 方法域)已交付:impl 方法分发、trait 默认方法体(空 impl)、impl prop、
  self 成员/方法互调、多参方法(input_ev2h.ct,228 cases);已裁定钉子:无 inherent impl、
  内建 to_string/slice 门控差异;
- C9g(绑定克隆与原地写)已交付:结构体绑定深克隆、类引用共享、self 可变方法可用、
  成员写原地透(input_ev2i.ct,229 cases);钉子:字符串载荷不可索引(本批踩中修复)、
  for 迭代绑定不克隆(镜像 rt)、结构体右值字段写克隆挂账;
- C9j①–④(C 代码生成器 → 自发射收官)已交付:`tools/trans_part.ct` 从 v0(I32 域)经
  v1(Str 域,0c558b9)v2(for/数组/浮点/assert,cef2a02)到 v3(List[Str] 引用语义/
  Atomic[I32]/索引读写/节点引用码 N/match-Option/read_file·byte_at·byte_slice/
  to_string/函数原型前置/尾值返回,含三修:ct_stmt 裸 return、尾槽发射、\n 转义透传);
  ladder 第 4 步 trans_v0–v3 往返全绿;**第 6 步自发射收官:cc.ct(159 decls)被
  Ctron 写的编译器+代码生成器完整发射为 9.9k 行 C,gcc 零错编译,产物即原生自举 cc,
  解释 input_cc3 输出与 C 版黄金逐字一致** —— 自举工具链自此可产出原生二进制。
- C9j⑥(CLI 化 + 宿主上位)已交付:发射产物驱动升为 `main(argc,argv)`,`run <file>`
  可覆盖输入锚(锚 = 自动探测 main 首个 read_file 字面量,默认烘焙)—— 产物即通用
  二进制。ladder 第 7 步:原生 cc 经 CLI 跑全部黄金(input_cc/2/3 + 负例拦截);
  第 8 步:固定点(编译器编译自身逐字节复现 + CLI 编译用户程序 == 黄金)。
  **C 宿主自此只剩"首次引导"职责。**
- C9j⑧(cc 求值器 Float 域)已交付:cc 求值器新增 D 值域(十进制定点,I64 尾数,
  fmt 镜像 rt fmt_val:整值 %.1f 否则 %g);发射器连带 I64 支持(类型码 "6")。
  浮点探针三方逐字一致(rt == cc 求值器 == 原生发射);bootstrap known-div 4→3。
  语言级发现:PascalCase 绑定名是解析歧义源(var Qq 绑定名被按变体模式收,探针实证
  小写同名全绿)——规范上应禁或三处统一(挂账)。
- C9j⑨(双种子差分清零)已交付:双解析器树 diff 证伪"解析嵌套挂账"(cc 树与
  宿主树全一致),真因是求值器作用域语义 —— rt 帧泄漏怪癖可观察地容忍 while 外读
  循环内绑定,cc 严格 env_drop 即 unbound。修复:run_block keep 参数(While 体绑定
  留存)+ env_dedupe/轮末压缩(防 O(n²))+ 发射器 hoist 保守化。bootstrap --full
  38/0/1、普通 ladder 39/0/0、make test 全绿。语言级发现:PascalCase 绑定名解析
  歧义(var Qq 绑定名被按变体模式收,探针实证小写全绿)—— 应禁或三处统一(挂账)。
- C9j⑩(原生形态 arena 内存管理)已交付:发射产物运行时全面切换 ctron_amalloc 凸分配
  arena(镜像宿主 rt 内存设计);原生自译化 cc×cc 此前 rc=137@63s 被内存杀,现 38s 完成、
  输出与黄金逐字一致(较种子 127s 快 3.3 倍)。ladder step 5 解除 bootstrap 跳过。
  **bootstrap --full 39/0/0 known-div=0 全绿 —— 自举版本可在自己的二进制上跑完整验收
  阶梯(用户要求的测试提醒节点)。**
- 下一步(可选):原生 cc 扫全
  suite_run/ev2 面;发射器全语言域(struct/闭包/并发/插值/值位 if·match)按"编译任意
  Ctron 程序"口径排期;浮点二进制舍入细节/f32 精度挂账;迁移 ASan 基建(见 HANDOFF)。
- 用法(独立驱动,推荐):
```bash
selfhosted/cc.sh <input.ct>     # parse → 语义 12 项 → 运行(正例 rc=0/负例诊断 rc=1)
```
- 注意:仓库 `make test` 暂被并行 C10-a 流(trans/suite_trans WIP)阻断,本目录逐 suite 构建运行可用。
- 详细路线见 `docs/superpowers/plans/2026-09-05-c-bootstrap.md`。
