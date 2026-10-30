# P2-C 构建配置系统 设计记录(spec-completion-roadmap 切片;spike 交付物)

> 创建:2026-10-30。上游:`docs/superpowers/plans/2026-09-13-spec-completion-roadmap.md` P2-C
> (§4.5 release 开关 / §5.4 采集关闭 / §8.5 预算清单)。状态对账(2026-10-28):
> 「CTCL v1 只落了清单语言,配置求值/发射开关未立项」——本记录立项。

## 1. 现状锚点(实码,非假设)

- **profile 面**:Rust 宿主 `sem::Profile { Full, Web, Bare }`(`compiler-rust/src/sem.rs:93`),
  字符串解析 `web/bare` 其余回退 Full;自举线 profile 同名三值(ctc.sh `--profile` 直传)。
- **开关现状**:命令行旗标零散——`ctc.sh` 环境变量(CTRON_STDPATH/CTRON_AST_CACHE/
  ANCHORVERSION)、`--target native|thumbv7em|wasm`、`--profile`、`-w` 宿主旗标;
  无文件化构建配置,无 per-profile 发射开关。
- **发射器形态**:bootstrap 发射 = `compiler/src/driver_emit.ct`(样板 println 流)+
  `trans_expr.ct/trans_stmt.ct/trans_conc.ct`(语句/表达式发射,字符串拼装);
  全局标志位今日不存在(发射纯函数式,无可变配置)。
- **预算清单(§8.5)**:编译期预算(深度/内存)在 seed 宿主 rt 侧硬编码
  (如 `ct_drn < 2048`、`ct_frn.pn < 128`);语言面无预算声明。
- **采集关闭(§5.4)**:诊断采集(diag 收集)恒开;CTCL 清单面有
  `ctcl_check` 校验但与构建配置正交。

## 2. 决策记录

| # | 题 | 裁决 | 理由 |
|---|---|---|---|
| D1 | 配置来源 | 三层:`Ctron.build.ctcl`(项目根,CTCL 文法) > 环境变量 > 命令行旗标;文件只载项目级默认 | CTCL 已是清单语言(v1 文法现成),零新解析器;CLI 覆盖文件是构建系统惯例 |
| D2 | 求值引擎 | 不引入语言级求值——CTCL v1 数据面(块式/四类值)够用;模板/插值拒绝(v2 再议) | 配置求值 = 组合爆炸源;闭源分发 D7 反投机律同哲学 |
| D3 | 键集 v1 | `profile`(full/web/bare)、`emit.checks`(i32 溢出/边界门)、`emit.budget{depth,frames,drops}`、`diag.collect`(bool)、`target` | 三 spec 键(§4.5/§5.4/§8.5)直接落位;其余键拒绝(E5070 未知名) |
| D4 | 发射开关实现位 | **trans 层标志结构**(`Trans/发射 env` 头部字段),非全局变量——自举线 = env 侧条目(`#cfg:*`),Rust 线 = `Trans` struct 字段;双镜同构 | 发射已是 env/struct 参数化;全局可变态破坏双镜镜像纪律 |
| D5 | i32 检查发射可行路径 | `emit.checks=on` → trans_expr 算术臂走 `ct_add/sub/mul(bits,us)` 既有运行时(已带宽度门);`off` → 直出 C 算术 | ct_add 族 v0 就绪(PREAMBLE 既有);开关只是选路,零新运行时 |
| D6 | 与 profile 关系 | profile = 预设包(bare ⇒ checks off + budget 收紧);显式键覆盖预设 | 单轴正交,组合显式化 |
| D7 | 传播 | CLI/文件 → ctc.sh 解析 → 环境变量 `CTRON_CFG_*` → 驱动(driver_emit 三入口)→ env 条目 → trans | 驱动已 env 驱动(W5b CTRON_AST_IN/FILL 先例);哈希仍在壳层 |

## 3. 设计

### 3.1 配置文件形

```ctcl
Ctron.build.ctcl
build {
    profile = full
    target  = native
    emit.checks = on        // §4.5:i32/边界检查发射
    emit.budget { depth = 512  frames = 128  drops = 2048 }   // §8.5
    diag.collect = on       // §5.4
}
```

校验 = ctcl_check 同源(未知名/非法值 → E5070/E5071,行号 = 块行)。

### 3.2 传播链(双镜)

```
ctc.sh: 读 Ctron.build.ctcl → ctcl_check 校验 → 平铺 CTRON_CFG_* env
  → driver_emit/driver_run/driver_check 三入口读 env → env_bind("#cfg:*")
  → trans_* 臂按码选路
Rust 镜像: Trans::new().with_cfg(Cfg) → trans 同名臂选路
```

缓存键(T20/W5b):`cc_<mode>.ct` 哈希已含驱动身份;**cfg 必须入键**——
`ctc_sha(源) + cfg 哈希12 + sha12(cc_<mode>.ct)`,否则配置变更吃陈旧缓存。

### 3.3 臂选路点(逐位)

| 开关 | 臂位 | on | off |
|---|---|---|---|
| emit.checks | trans_expr 算术(Binary Add/Sub/Mul 臂) | `ct_add(a,b,32,0)` | `(a + b)` |
| emit.budget | driver_emit 样板常量(ct_drn<2048 等替换为 cfg 注入宏) | cfg 值 | spec 默认 |
| diag.collect | driver_check 采集循环 | 全采 | 仅 error 级 |

## 4. 任务切分(建议三批)

1. **W1 配置面**:Ctron.build.ctcl 解析复用 + E5070 族诊断 + ctc.sh 平铺 +
   缓存键入 cfg 哈希;验收 = smoke 新腿(配置变更 → 键变 → 重译)。
2. **W2 emit.checks**:双镜算术臂选路 + ct_add 族位宽贯通(i32 语料差分);
   验收 = 同语料 on/off 双产物 cc 级行为差(溢出 panic vs 回绕)。
3. **W3 budget/diag**:样板常量参数化 + 采集面;验收 = 预算触顶 panic 消息
   含预算名;meta_check 13 在册债不清(正交)。

## 5. 风险与开放题

- 双镜漂移:cfg 选路臂双写,以 fx_* 三方对拍(close-out 惯例)钉住。
- WasmGC 证伪同款:若 bare 预设与 checks 语义冲突(bare 无运行时门),
  bare ⇒ 强制 off 并校验期报 W(不静默)。
- 开放:`Ctron.build.ctcl` 与 `Ctron.ctcl` 清单分文件还是同文件 `build {}`
  块——倾向同文件(单清单源),W1 开工首日定谳。
