# derive(Json) 插件充实计划——to_json 全型别 + from_json 读入半边

> 状态:**已裁决开工(2026-10-10 用户裁决:五点全按推荐执行——A F64 排除 / B Option 入(Some→值/None→null) / C from_json 入 W2 / D T[N] 非目标 / E enum 非目标)。W0 起批。**
> 上游:T52 插件协议(spec `docs/superpowers/specs/2026-10-03-t52-plugin-sandbox.md`,dc16a88d 落库,官方锚 `tests/plugins/json_demo` 双臂绿);
> JIx 索引面(fc8359a5)——W2 from_json 的查询基底;
> 修改面:`compiler/plugins/derive_json/`(插件包)+ `tests/plugins/`(锚)+ spec §8.3 修订注(契约不动则只补记)+ `lib/std/json.ct`(W2 前置 `jsub` 一件)。

## 0. 裁决记录(规范性)

1. **机制走 T52 插件协议,不建新工具链**(2026-10-09 对话实证:官方 json_demo 即 derive(Json) 端到端,双臂 CI 门在);「ctron-ast dump + 外置脚本 + 提交生成文件」方案正式撤回——提交期产物存在陈旧类缺口(struct 演化生成件静默滞后,无门可拦),属半 hack;插件展开钩子每次构建现算,缺口在机制上不存在。
2. **产物形态维持方法形自由 fn**(`fn to_json(self: T) -> Str`,UFCS 调用);trait impl 发射在册缺口不归本计划。
3. **to_json 产物零 use 自包含**(转义 helper 由插件合成,不引 std.json);**from_json 产物消费方须 `use std.json.{...}`**(跨包引用即请求铁律;名字由消费方 use 供给,产物文本自身仍零 use)——此口径差异随批写 spec §8.3 修订注。
4. **插件对 std 零依赖**:转义全量移植 `std/json esc` 语义(引号/反斜杠/控制字符 `\u00XX`),插件内自含。
5. **未支持型别 → 空产物 → E5060.iface**(v1 无诊断通道口径);清除现 demo 的 `DBG tn=...` 调试残留。

## 1. 型别覆盖矩阵(v1 终态)

| 型别 | to_json | from_json | 依据/备注 |
|---|---|---|---|
| I32 / I64 | `to_string()` | `jk_i64` 预检 + `jv_i64` | kind 三分沿用 jget 码表 |
| Bool | if 表达式两臂 | `jk_bool` + `jv_bool` | |
| Str | 全量转义(修 demo 只转引号/反斜杠的缺口) | `jix_val` 原文 | RFC 8259 合规 |
| F64 | **裁决点 A** | 同左(若裁入) | emit `to_string` 乱值债 + F64 文本格式语言侧未钉 |
| 嵌套 struct(亦 derive) | `self.f.to_json()` | `jsub` 子树递归 hydration | chk 拦未 derive 的字段型别 |
| List[T] | `[...]` 逐元素递归 | `jw_keys` 下标枚举逐元素 | jw_keys 已在写半边,零新增 |
| Option[T] | **裁决点 B**(Some→值 / None→null) | `jix_tag=="z"`→None | null 是 Option 的 JSON 母语 |
| T[N] 定长数组 / Map / enum / Result | 非目标(§5) | 同左 | |

## 2. 波次

### W0 探针批(先行,不落主面)
- `DeriveInput.fields[].ty` 文本形实证:`List[Str]`、`List[List[Str]]`、`Option[I32]`、嵌套 struct、F64 在 ty 槽的确切文本(空格/嵌套括号形态)——插件字符串手术的输入契约。
- **同名方法冲突探针**:同文件两个 `@derive(Json)` struct,`fn to_json(self: T)` 是否按接收者型别分派(不冲突)或撞名(E5030 系)。撞名则产物名改 `<小写T>_to_json` 形,§1 矩阵同步改。
- 多模块 pkg:struct 在非 main 子文件时产物注入位与可见性。
- 自引用 struct 死递归行为实证(文档化为非目标)。
- **match-on-Result 发射缺口边界**(ndjson x2 臂 SKIP 同源):std/json 本体 match 在 emit 臂全绿、corpus 单文件红——用生成代码形态跑双臂探针定边界。
- T52 八债逐条核对(`2026-10-03-t52-plugin-sandbox.md` 台账),本计划触及项列出。
- 门:探针记录回填 §6;产码不入 lib/compiler 主面。

### W1 to_json 充实(裁 A/B 后动)
- 转义 helper 全量化(控制字符/`\u00XX`;demo 现形不合 RFC 8259)。
- 嵌套 struct/List[T]/Option[T](按裁决)逐元素递归;生成代码保持平铺串接形态(demo 同款)。
- 门:`tests/plugins/json_demo` 扩为 derive_json_suite(interp/emit 双臂;金样:中文/转义往返/嵌套/List/Option);负锚:未支持型别 E5060.iface;evil_derive/sandbox_escape 回归。

### W2 from_json 读入半边
- **前置一件(本泳道随批)**:`lib/std/json.ct` 增 `jsub(entries, path) -> List[List[Str]]`(子树条目提取、重根;零 use,与 JIx 同风格,~15 行 + 测试)——嵌套 struct hydration 的基底;List 走既有 `jw_keys` 零新增。
- 产物:`from_json(text) -> Result[T, Str]`:parse → `jix_new` → 逐字段 `jk_*` 预检(kind 非 ok 即 `Err(kind@路径)`)→ `jv_*` 取值 → struct 字面量。**生成代码禁 match-on-Result**(`jk_*`/`jv_*` 组合子形态,绕开发射缺口,W0 定界后复核)。
- 嵌套:`Sub_from_json` 接 `jsub` 子条目;List 元素逐下标递归。
- 门:双臂 round-trip 恒等(to_json→from_json,含转义往返/中文/嵌套);负锚:坏 JSON / 缺字段 noent / 型别不符 type 三 kind。
- 消费方契约:`use std.json.{ parse, jix_new, jix_tag, jix_val, jk_i64, ... }`(§0-3)。

### W3 收尾
- spec §8.3 修订注(产物 use 契约/覆盖矩阵终态);COVERAGE、divergences 回写;`tests/plugins/run.sh` 锚数更新。
- 门:plugins 全锚 + json_fidelity(jsub 随批)+ 伞门抽验 + fmt 净 + std_doc --check(若 pkg 文档注释动)+ mkdocs strict。

## 3. 验收总门(每波全绿即落库,pathspec 限定)
- 双臂(interp/emit)是硬门;确定性:同输入两次展开逐字节一致。
- 每批随门:tests/plugins/run.sh 全锚;动 lib/std 则 json_fidelity + stdpkg 副本随批。

## 4. 风险与已知雷
1. `DeriveInput` v1 冻结面:不加字段;泛型解析全在插件内做字符串手术(ty 文本含空格风险 W0 实证)。
2. 插件无诊断通道:未支持型别的错误 UX 差是 v1 已知口径,协议扩展不做。
3. 自引用 struct = 死递归:文档非目标(与 to_string 同命运)。
4. F64 双债(emit to_string 乱值 + 文本格式未钉)是裁决点 A 的依据,裁入则两债随批登记豁免面。
5. match-on-Result 发射缺口:W2 生成代码全组合子形态规避,W0 定界兜底。
6. 展开钩子在编译器热区(driver_check/run/emit 三处):本计划不改钩子,只改插件包与锚,合并面风险最小化。

## 5. 不做清单(非目标,登记)
- enum 序列化形态(tag 串/嵌套对象)——独立设计题。
- Map/HMapS 序列化(迭代序虽有宪章 8 保证,键转义 + 嵌套泛型 ty 手术成本高)。
- trait impl 发射、derive 协议 v2(诊断通道/DeriveInput 加字段)——各自归属原泳道。

## 6. W0 探针记录(2026-10-10 实测回填)

1. **ty 有损实锤**:`DeriveInput.fields[].ty` 塌缩头标识符——`List[Str]`/`List[List[Str]]`/`Option[I32]` 全部只余 `"List"`/`"Option"`,型别实参不可达;嵌套 struct 名完整(`W0Inner`)、`mutable` 槽正常。**List/Option 序列化超 v1 接口 → 挂账 T52 iface v2 修订触发件(§2.2 程序),W1/W2 不含**。
2. **同名方法跨接收者型别冲突**:interp 方法表按名字分派、首注册胜——两 struct 均 derive 时 `p2.to_json()` 派发到 P1 实现(panic `field:s`)。**产物必须型别 mangling:`<T>_to_json`**。
3. **字面量接收者方法调用两臂皆坏**(interp panic `field:s` / emit 静默错编;坑 71② 同族)——消费方先绑变量再调;产物以 `self:` 形参形不受影响。
4. **现插件 DBG 残留产物 = interp SIGSEGV(139)**(未支持型别路径);**空产物 = 静默通过**(插件头注「空产物报 E5060.iface」与行为不符,登记分歧)——v1 告警信道定为陷阱 fn(sem 未解析调用嵌型别名,json_demo 负锚)。
5. **utf8_enc 解释臂传参 bug**(新登记分歧):`utf8_enc(123)` interp 臂得替换符 3 字节、emit 臂正确得 `{`;std/json 写出半边金样两侧同用故自洽掩蔽。产物转义改纯字面量形(std/json esc 同款),不用 utf8_enc。
6. **组合子边界**:parse/is_some/.or/jix_* 与 match-on-Result 传播形态双臂全绿(ndjson x2 红为别形态)——W2 生成代码可用 match 传播。
7. **发射路形态**:裸 `ctron-emit` 直发体丢失复证(坑 73);插件感知正规形 = `ctron-emit run <file>`;`ctc.sh emit` 不读 pkg 清单(插件场景不可用)。插件 path 基准 = 入口文件所在目录(`dir+"/../"+relpath`,json_demo 三层 `../` 之由);插件包自持 Ctron.ctcl。
8. **随批修存量死循环**:std/json `junesc` 已知转义分支(\t \n \r \" \\ \/)不推进下标 → 无限循环 OOM(SIGKILL 137;json_fidelity 无已知转义直测面故存量绿)——两字符消耗归一 + 两枚测试钉(json 模块/解析路径各一)。
9. T52 在册债核对:三线 parity(plugin 块 C/Rust 校验器、接口包 use 解析)与 trait impl 发射缺口均不触本计划;新增登记 = 探针 4/5 两分歧。

## 7. 待裁决(用户,五点)
- **A. F64**:推荐 = 排除(空产物 E5060.iface + 文档注记),等 emit to_string 乱值债与 F64 文本格式钉形后再入;备选 = 插件内自写定宽格式(把语言未钉债变成插件私钉,不推荐)。
- **B. Option[T]**:推荐 = Some→值 / None→null;备选 = 不支持。
- **C. from_json 入本计划**:推荐 = 入(W2;JIx 刚落库正成热灶);备选 = 另立计划只做 to_json。
- **D. T[N] 定长数组**:推荐 = 非目标(数组字面量发射治红前不碰数组道);备选 = 与 List 同形。
- **E. enum**:推荐 = 完全非目标登记;备选 = 字符串 tag 形有限支持。
