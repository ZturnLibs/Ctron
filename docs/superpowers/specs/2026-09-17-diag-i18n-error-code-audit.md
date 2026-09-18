# 诊断 i18n 设计与错误码体系审计

> 日期:2026-09-17 · 状态:设计稿(待评审)
> 范围裁决(已定):仅 Ctron 自产诊断做 i18n;C 侧报错(cc/链接器 stderr、errno、FFI 外来错误)原样直通,永不翻译。
> 数据基线:自举线 `compiler/` 当前工作区;审计口径 = 四方对账(spec §10.1 / meta_check `ERROR_CODES` / tests/README §4 / 实际发射)。

---

## 1. 范围裁决(定案)

| 对象 | 处置 |
|---|---|
| 自举编译器 E/W 诊断(39 个码 / ~84 处直推 + pdiag 间接) | 入目录,i18n |
| lex 词法诊断(现走 `行:列 码:` 变体格式) | 入目录(P1 保留原格式) |
| C 侧报错:cc/链接器 stderr、errno、FFI 外来错误 | 原样直通 |
| 编译器内部 panic(`trans_*` 的 "ct_match_value:pat" 类) | 不入目录(开发者面) |
| 解释器运行时 panic("no fn:"/"print arity" 类) | 不入目录(bug 面,§5.5);挂账 |
| 发射产物内嵌运行时文案("integer overflow (+)") | 现状即英文;挂账 |
| 宿主线 compiler-c / compiler-rust 诊断 | 本次不动;码位对齐进 §3.4 |

## 2. 现状测绘

### 2.1 发射面

诊断 push 精确分布(已剔除非诊断的值 push,共 84 处):
`sem_main` 25 / `sem_type` 19 / `sem_comptime` 8 / `sem_calls` 7 / `parse_pkg` 6 / `sem_walk` 5 / `sem_alloc` 4 / `lex` 4 / `sem_move` 2 / `sem_spawn`·`sem_pure`·`sem_own`·`sem_exh`·`sem_closure` 各 1;另 `parse_decl` 等 4 处经 `pdiag()` 间接发射(E1001)。

**输出格式三变体**(P1 全部逐字节保留):

1. 主流:`file:CODE@行: 文案`(diag 直推)
2. lex:`file:行:列 CODE: 文案`(`lex.ct:151`,位置前置)
3. 无行号:`file:CODE: 文案`(W8030,`sem_calls.ct:193`);Enum 边界行号退化为 `E4041@0`

### 2.2 注册表面(四方对账结论)

- **spec §10.1**(34 码,权威):缺 E4041/E4042/E4044/W8051/W8052(v0.6/v0.7 FFI 族,零收录)、缺 E5030;E6030 为唯一"注册未发射"(预留,合理)。
- **meta_check `ERROR_CODES`**:缺 E2040、W8030、W8040(spec 有/已发射);多 E4041/E4042/E4044(spec 无);**E4044 重复键**(两行异文,Python 静默取后者)。
- **tests/README §4**:声明"与 meta_check 保持同步",抽样窗口内同样不见 E2040/W8030/W8040/E5030 → P0 对齐时全量核对。
- **实际发射**:39 码,其中 6 个(E4041/E4042/E4044/E5030/W8051/W8052)spec 未注册 → 直接违反 §10.1"新增码先进本表再使用"。

### 2.3 消费面

- `tests/`:`//@ fail:` 按码断言(36 锚:E1001×5、E3040×4、E3010×3、E6010/E4050/E4010/E3020/E2030/E2020×2,其余 ×1);`//@ msg:` 31 锚钉文案子串(多为 "allocation"/"Send"/"Drop" 中性词,至少 1 个中文锚"无法推断")。
- `compiler/test/`:suite.py + smoke.sh 独立车道(fx_litfit 四件经 `tc_fx ... "E2040"` grep 输出,无 `//@` 标记)。
- JSON 面:`driver_check.ct` 从文本**反解析**(剥 `@<数字>`、`probe_line` 用文案尾段定位行);span 列恒 1,notes/fixes 恒空(§10.2 承诺未兑现)。
- 三线差分:对发射产物/AST 逐字对数,诊断文案不进差分面。

## 3. 错误码审计

### 3.1 分类完整性

- **F1 发射未注册 ×6**:E4041/E4042/E4044/W8051/W8052(FFI 族,meta_check 已收、spec 零收录)+ E5030(`parse_pkg.ct:264` use 撞名,两边均未收,亦无测试锚)。
- **F2 注册标"预留"实已发射 ×8**:E3031/E3060/E3070/E4010/E4030/E6010/E5010/E5020 的 spec 锚点列仍写"预留"→ 表陈旧。
- **F3 meta_check 漏收 + 重复键**:E2040(spec 有 + 3 发射点 + 4 个 fx 测试锚 + smoke 锚,漏得最重);W8030/W8040;E4044 重复键。
- **F4 死码 W8053**:meta_check 注册、全仓库零发射,语义已被 v0.7"extern 返回 fn 合法化"取代 → 注记 dormant,码位按"只增不改"保留。
- **F5 词法层零细分**:禁用标点 `::`/`;`、非法字符、未闭合串等全部压 E1001,词法域无自有码;且 lex 格式是位置前置变体(2.1)。
- **F6 运行时/eval panic 无码体系** → 裁决:panic 属 bug 面(§5.5),不入码表,不入 i18n v1。

### 3.2 同类内聚

- **C1 E2010 杂物码(17 发射点,≥4 语义族)**:
  - 族 a 值类型不匹配(let/赋值/返回/实参/const)——正统本义;
  - 族 b **实参数不匹配(arity)** ×3(`sem_type.ct:477/572/577`)——非类型错误 → 新码 **E2080**;
  - 族 c 运算符/条件操作数约束 ×7(Str 算术、Str 拼接、Bool 算术、`&&`、`||`、if 条件、while 条件)→ 新码 **E2081**;
  - 族 d `?` 传播误用(`sem_type.ct:686`,§5.3 违规)→ 新码 **E2082**。
  - E2010 本义冻结不变;拆分走 §10.7 流程(先表+锚更新,再改发射)。当前仅 1 个 E2010 测试锚,是迁移成本最低的窗口期。
- **C2 E2020 混模块域**:`parse_pkg.ct:222/224/276`(use 未找到符号/未公开/模块不可读)是模块域错误却挂名称解析码 → 迁 E5 段(码位见 §3.4),E2020 收敛回纯名称解析。
- **C3 同码多点手写文案漂移**:E2010 arity 三处三文案;E3040 两分支仅前缀差 → i18n 目录"同码同模板"天然根治。
- **C4 去重探针失效 bug**:`sem_comptime.ct:22` 的 in_list 探针 `"E6020: ..."` 与实际发射 `"E6020@行: ..."` 格式不符 → 去重永不命中。helper 单一出口 + `diag_once` 根治。

### 3.3 分布合理性

- **D1 E4"效果"段被 FFI/ABI 族占用**:E4040(信任标注)/E4041(repr)/E4042(回调)/E4044(变参)皆非效果语义。裁决:**新开 E7 段 = FFI/ABI**;存量 E404x 冻结原地("永不改义"),此后 FFI 新码(cimport 绑定、dlopen、`#[link_name]`、导出面——ffi-analysis 缺口 #1/#2/#4 全在排期上)一律进 E7,防止 E4 撑成杂物段。
- **D2 E5 段三线各说各话**:宿主线 E5040–E5050(11 码)实为 **CTCL 配置语言诊断**("内联表不支持/块未闭合/字符串未闭合"等),并非模块错误;自举线 E5 仅 E5010/E5020/E5030。裁决:宿主已发布的 E504x 冻结不动,但 §10.1 表注必须写明 **E5 = 模块(E5010–5030)+ CTCL 配置(E5040–5050)双域**;未来可规划 E8(配置语言)作迁移目标。自举 use 域新码(C2)落 E506x。
- **D3 编号惯例**:主流 ×010 步进、段内留隙良好;E1001/E5030 为历史散点(冻结)。新码一律回归 ×10 步进。
- **D4 密度**:E2×9 / E3×9 / E4×5 / E5×3 / E6×3 / W8×7,余量充足;E1 仅 1 码(F5 词法细分挂 v2)。

### 3.4 审计裁决汇总

| 动作 | 码 | 时机 |
|---|---|---|
| 补注册(spec + README§4 + meta_check) | E4041/E4042/E4044/W8051/W8052/E5030 | P0 |
| 补注册(meta_check + README§4) | E2040/W8030/W8040;E4044 去重 | P0 |
| 注记 dormant | W8053 | P0 |
| 预留行锚点刷新 | E3031/E3060/E3070/E4010/E4030/E6010/E5010/E5020 | P0 |
| 新段定义 | **E7 = FFI/ABI**(自 E7010 起) | P0 |
| 段注 | E5 双域说明 + E504x = CTCL 归属 | P0 |
| 新码(拆分,走 §10.7) | E2080 arity / E2081 操作数类型 / E2082 `?` 传播 | P2 后择机 |
| 新码(迁移,走 §10.7) | E5060 use 符号不存在 / E5061 use 符号未公开 / E5062 模块文件不可读 | P2 后择机 |

## 4. i18n 详细设计

### 4.1 目录与模板

- 新文件 `compiler/src/diag_msg.ct`:`fn diag_tpl(code: Str, en: Bool) -> Str`,zh/en 双表;查无 → en 回落 zh 原文(比半成品英译诚实),zh 缺条目属 bug(由 4.4 R1 在 CI 拦截)。
- 模板槽位 **`%0`/`%1`…`,`%%` 转义字面 `%`**。槽位不用 `{}`:Ctron 字符串插值(§1)`{expr}` 会吃裸花括号(现存文案即含 `\{`),`%` 槽零转义成本。
- 标识符/类型名等动态量永远作实参传入,绝不写死进模板。
- 文案迁移动作是**逐字搬运**(zh 表 = 现文案原样),不趁机润色——P1 字节不变是硬验收。

### 4.2 发射 helper(单一出口)

```ctron
fn diag_fmt(code: Str, line: Str, args: List[Str]) -> Str  // 查表+插值 → "CODE[@行]: 文案"
fn diag(diags: List[Str], code: Str, line: Str, args: List[Str])         // push
fn diag_once(diags: List[Str], code: Str, line: Str, args: List[Str])    // in_list 去重后 push
```

- `line` 传 `""` → 无 `@行` 段(W8030 形态);主流与无行号两变体由 diag 覆盖。
- **lex 位置前置格式 P1 原样保留**:`lex.ct:151` 的 `pdiag` 只把 msg 换成 `diag_fmt` 结果,拼装顺序不动;格式统一留 P3 规范修订(须与 JSON span 结构化同批)。
- 现存手工去重点(`sem_type.ct:469` 等)迁 `diag_once`:探针与发射同源,根除 C4 类格式漂移。

### 4.3 语言选择(构建锚,顺应现状)

- 编译器无 argv 解析、无 getenv;`--format=json` 即以构建锚注入(`ctc.sh:59` 的 sed 手法)。i18n 同款:**`ANCHORLANG`**("zh"/"en",缺省 zh),`sed -e "s|ANCHORLANG|zh|"`;非 sed 路径字面 "ANCHORLANG" 视为 zh(与 ANCHORFMT"非 sed 恒文本面"同款宽容)。
- 缺省 **zh** 的理由:31 个 `//@ msg:` 锚与中文文档语境零扰动;§10.2 冻结的是 JSON schema 不是 message 语言(agent 消费码);en 为 opt-in。
- smoke 增一条 en 冒烟:一个已知码的 en 文案断言 + 一个"en 缺条目回落 zh"断言。

### 4.4 机器面与守卫(meta_check 增项)

- **R1 目录覆盖守卫**:diag_msg.ct zh 表码集 ⊇ (ERROR_CODES ∩ 实际发射码集)——防"加码忘翻"。
- **R2 注册表修复**:补 E2040/W8030/W8040/E5030;E4044 去重;四方(spec §10.1 / meta_check / README §4 / 发射)一次性对齐。
- **R3**(P0 后启用):`//@ fail:/warn:` 锚 ∈ spec §10.1。
- **R4**:`//@ msg:` 锚建议中性子串(现存的中文锚在缺省 zh 下合法,不破坏)。
- JSON 面 schema 不动;message 字段内容随 lang;`probe_line` 反解析在 P1 字节不变前提下不受影响。

## 5. 迁移计划

**P0 规范修订**(§10.7:先文档+锚,后实现;一个提交,零行为变化):
§10.1 补 6 行 + 预留行锚点刷新 ×8 + W8053 dormant 注记 + E7 段定义 + E5 双域段注;meta_check/README §4 同步(R2)。

**P1 等价重构**(验收 = zh 输出逐字节不变):
diag_msg.ct(zh 表)+ helper 落地,按批发射点改造:
① `sem_main`(25) ② `sem_type`(19) ③ `sem_comptime`+`sem_calls`+`sem_walk`(20,顺手修 C4) ④ `parse_pkg`+`sem_alloc`+`sem_move`+`sem_spawn`+`sem_pure`+`sem_own`+`sem_exh`+`sem_closure`(16) ⑤ `lex` pdiag(4+间接)。
每批验收:suite.py + smoke.sh + meta_check 全绿,并 diff 改造前后诊断输出确认零漂移。
(可选独立提交:出口结构化后,driver_check 的 JSON 面改为直接消费结构化诊断,兑现 §10.2 notes/fixes——不塞进 P1。)

**P2 真 i18n**:en 表 + `ANCHORLANG`(ctc.sh/build.sh)+ meta_check R1 + en 冒烟。

**P3 挂账**(各自独立立项):词法细分码(E1010 非法字符/E1020 禁用标点/E1030 未闭合串…)与输出格式统一修订;拆码迁移 E2080/81/82、E506x;JSON notes/fixes 兑现;宿主线目录移植;运行时 panic 文案;`//@ msg:` 锚中性化。

## 6. 风险

| 风险 | 对应 |
|---|---|
| P1 改造漏点导致行为漂移 | 字节不变硬验收 + 每批全绿门 |
| `%` 与现存文案冲突(如 "85%") | `%%` 转义约定;落地时 grep 复核模板 |
| en 半翻译比不翻译更糟 | 缺条目回落 zh + R1 守卫 |
| E7/E5 段裁决与宿主线漂移 | P0 段注先进 spec,三线各按表对齐 |
| ANCHORLANG 非 sed 路径漏替换 | 字面 token 视为 zh(与 ANCHORFMT 同款) |
| `//@ msg:` 中文锚未来阻碍 en 缺省 | R4 立规矩:新锚一律中性子串 |
