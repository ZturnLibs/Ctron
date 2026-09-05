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
| `cc.ct` | **统一 cc 驱动**(自足快照:parse→单文件语义 12 项→run):有诊断输出 `CODE: msg` 并止;干净则解释执行 main/test | C9c+C9e① seq=9 正/负夹具与 C 管线逐字一致 |
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
- 下一步:trait/impl 方法域(用户类型方法调用与 trait 默认方法),或 cc.ct 快照刷新并入 C9f①,
  或元组/Try 之外的运行域小面(如 slice 语法糖),或把 `selfhosted/` 提升为独立工具链。
- 注意:仓库 `make test` 暂被并行 C10-a 流(trans/suite_trans WIP)阻断,本目录逐 suite 构建运行可用。
- 详细路线见 `docs/superpowers/plans/2026-09-05-c-bootstrap.md`。
