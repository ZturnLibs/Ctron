# §10 诊断契约与符合性

## 10.1 错误码注册表(v0.3 主注册表)

分段:E1xxx 解析 / E2xxx 类型 / E3xxx 内存与并发 / E4xxx 效果 / E5xxx 模块 / E6xxx comptime / W8xxx lint。

| 码 | 含义 | 规范依据 | 测试锚点 |
|---|---|---|---|
| E1001 | 解析错误(通用语法违规;含比较不可链 §4.3) | §1 | `01c_parse.neg.ct` |
| E2010 | 类型不匹配 | §3 | 通用 |
| E2020 | 未解析的名称 | §2.4 | 通用 |
| E2030 | match 不穷尽 | §4.6 | `02_match_exhaustive.neg.ct` |
| E2050 | bound 不满足(泛型实参不满足型参 bound,诊断携带实参型别名) | §3.9.2(v0.6) | `compiler/test/fx_bound_neg.ct` |
| E3010 | spawn 捕获非 Send | §7.4 | `06_spawn_nonsend.neg.ct` |
| E3020 | channel 收发非 Send 类型 | §7.4 | `06_channel_nonsend.neg.ct` |
| E3030 | `static var` 不存在(解析器对 `static var` 做恢复并专门产出本码,而非 E1xxx——对 AI 迭代友好) | §7.6 | `06_static_var.neg.ct` |
| E3031 | 非 Send 类型作为全局/静态存储 | §7.4 | 预留 |
| E3040 | no_alloc 上下文出现 GC/String 分配 | §6.5 | `05_own_alloc.neg.ct` / `08_bare_alloc.neg.ct` |
| E3050 | own 块 move/borrow 违规(含 use-after-move) | §6.3 | `05_own_move.neg.ct` |
| E3060 | own 块内对 GC 值可变借用 | §6.3 | 预留 |
| E3070 | 闭包可变捕获未显式 `Mutex[T]` 包装 | §4.7(v0.6 草案) | `roadmap/r3a_capture_var.neg.ct`(预留) |
| E4010 | 能力使用超出 manifest 声明 | §8.2 | 预留 |
| E4020 | `#[pure]` 含副作用 | §8.3 | `07_pure.neg.ct` |
| E4030 | `#[no_spawn]` 上下文 spawn | §8.3 | 预留 |
| E5010 | trait 孤儿规则违规 | §2.5 | 多文件(预留) |
| E5020 | 循环依赖 | §2.6 | 多文件(预留) |
| E6010 | comptime 预算超限 | §8.4 | 预留 |
| E6020 | comptime 副作用/不确定 | §8.4 | 预留 |
| E6030 | comptime 反射泛型运行时类型(parametricity) | §8.4 | 预留 |
| W8010 | struct 含可变类引用字段(拷贝浅共享) | §6.1 | `03_shallow_copy.lint.ct` |
| W8020 | must-use 结果被丢弃(Result/Option) | §5.6 | 预留 |
| W8030 | 未使用绑定 | — | 预留 |
| W8040 | 遮蔽前奏符号 | §3.8 | 预留 |

- 码一经发布**永不改义**;废弃只增不改;新增码先进本表再使用(与 `tests/meta_check.py` 注册表同步)。
- 每条诊断必须含:稳定码、人读消息、**机器可执行修复建议**(fix-it)。

## 10.2 JSON 诊断契约(`ctron check --format=json`)

面向 agent 循环消费的第一接口,格式冻结:

```json
{
  "diagnostics": [{
    "code": "E3010",
    "severity": "error",
    "message": "closure captures non-Send value `c`",
    "file": "src/main.ct",
    "span": {"line_start": 12, "col_start": 20, "line_end": 12, "col_end": 25},
    "notes": ["`Cell` has a `var` field `n` and is confined to one task"],
    "fixes": [{"title": "wrap in Mutex", "edits": [{"kind": "replace", "span": {...}, "text": "Mutex[Cell](...)"}]}]
  }]
}
```

- `severity ∈ error|warning`;`fixes[].edits.kind ∈ replace|insert|delete`;span 为 1-based。
- 同一次 `ctron check` 完成 parse + 类型 + Send + 分配效果 + lint,一次返回全部诊断(P3/§8.3)。

## 10.3 确定性与可复现

- `ctron test --deterministic`:冻结调度序与哈希种子;并发测试失败可复现。
- 构建内容寻址缓存:同输入同产物(跨机可复用)。

## 10.4 doc-test

- `///` 文档注释中的代码块**编译并运行**(失败 = 测试失败);文档即回归,服务人审 AI 产物的"立即可验证"。

## 10.5 规范↔测试符合性映射(总表)

| 规范章 | 主题 | 锚点测试 |
|---|---|---|
| §1 | 词法/语法 | `01_basics.ct`、`04_generics_comptime.ct` |
| §3/§6 | 值/引用/Box/浅拷贝 | `03_values_refs.ct`、`03_shallow_copy.lint.ct` |
| §4/§5 | 表达式/错误模型 | `01_basics.ct`、`02_option_result.ct`、`02_match_exhaustive.neg.ct`、`01_overflow.panic.ct` |
| §6 | own/bare/分配效果 | `05_own.ct`、`05_own_alloc.neg.ct`、`05_own_move.neg.ct`、`08_bare.ct`、`08_bare_alloc.neg.ct` |
| §7 | Send/并发 | `06_concurrency.ct`、`06_spawn_nonsend.neg.ct`、`06_channel_nonsend.neg.ct`、`06_static_var.neg.ct` |
| §8 | 能力/纯度 | `07_capabilities.ct`、`07_pure.neg.ct` |

符合性定义:**P1 实现达标 = 上表全部测试按 `tests/README.md` 规则通过**。多文件用例(模块/孤儿/FFI/包)随 P1 基建补入,格式不变。

## 10.6 插件扩展点(诊断相关)

- lint 插件:输入类型化 HIR,输出带注册码的诊断(码段 W9xxx 预留给第三方);沙箱执行、确定性、可缓存(§8 插件原则)。
- 第三方码必须落在 W9xxx;E 段为语言保留。

## 10.7 冻结清单回顾

v0.3 规范冻结 = 本文档 §1–§9 normative 项 + §10.1/§10.2 契约。实现阶段的规范修订走"先改本文档 + 增补测试锚点,再改实现"的顺序(测试先行)。
