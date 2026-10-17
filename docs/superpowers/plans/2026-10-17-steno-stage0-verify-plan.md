# Stage 0 计划 —— steno-verify 纯计算 dogfood(Ctron 侧)

- 状态:执行中(2026-10-17)。
- 契约:`docs/superpowers/specs/2026-10-17-steno-verify-format-spec.md`(stenolog.v1 / stenolicense.v1,已定案)。
- 背景:steno-gap-fill-design.md §11「Stage 0 不等 G 批」;G1–G7 已全落库,本计划 = 采纳路径第一棒的 Ctron 侧交付物。「秘本侧计划可见」前置以规格落库方式自就(格式契约即双侧合同)。

## 交付物

`steno/verify/` 项目(steno-verify 单二进制)+ `tests/steno/` 验收门。能力依赖(全部已在库,零编译器改动):

| 依赖 | 面 | 发射臂状态(2026-10-17 探针实证) |
|---|---|---|
| lib/crypto | ed25519 sign/verify/keypair、sha256、rand | ✅ tests/crypto 同链 |
| std/json | parse/qval/qtag(展平条目) | ✅ 但有四条形状纪律(下 §纪律) |
| std/str、std/enc | index_of/rindex/split/lines、hex_encode/decode | ✅ |
| fio | read_file(发射=Str,空=缺)、write_excl_0600、write_raw | ✅ |
| T26 argv | `#[trusted] extern "c" ctron_prog_args_n/at` 直调 | ✅ 裸执行=argv[1..] |
| 多模块 | `use <pkg>.<module>.{…}` + main 薄壳(20480B 直发家族绕行) | ✅ |

## 波次

- **W1 骨架+密钥**:steno/verify 项目(Ctron.ctcl name=steno_verify;src/main.ct 薄壳)+ hexutil/util + keygen + selftest;emit+cc 出二进制,手跑冒烟。
- **W2 审计链**:chain.ct(core 提取/逐行验证/链环)+ sign-chain(夹具生产器);正例自签自验。
- **W3 license**:license.ct 验证 + sign-license;机器绑定/新鲜度/strict 面;CLI 收口(--json/--quiet/退出码 0/1/2)。
- **W4 验收门**:tests/steno/run.sh——openssl 双向对拍(A 我签他验/B 他签我验)+ 负例矩阵七枚 + selftest;门绿后 docs 收尾(COVERAGE 登记 + 批终提交)。

## 发射臂形状纪律(本批探针实证,写码必守)

1. 字符串字面量:`{` 必须 `\{`;`}` **不**可转义(`\}` 即 E1001)。
2. 命名空间函数一律 `use std.json.{parse,qval,qtag}` 裸名调用;`json.parse(...)` Member 形在发射臂生成坏 C(t_json/t_parse 未声明)。
3. 容器载荷 Result **禁 match 绑定臂**(Ok(es) 绑 List 段错误;在册「match-on-Result 发射缺口」的窄化:标量载荷绑定可用)——判别走 `is_ok()` + `let es = pr.expect("…")` 先绑后用,**expect 严禁内联进调用实参**(丢型别,int→指针错)。
4. match 臂体必须块形 `{…}`(裸表达式即 `ct_stmt:match R arm body`)。
5. Try(`?`)不认命名空间调用(把包名当局部变量);经本地包装 fn 亦受 2 连坐——本批不用 Try,判别全走 3。

## 验收门口径

- `tests/steno/run.sh`:emit(steno/verify/src/main.ct 依赖路径)→ cc(crypto c_src + openssl@3)→ 逐例跑;断言 = 退出码 + 输出 grep;openssl 对拍双向(§5 契约);负例矩阵全拒。
- 纪律:门内不带 CTRON_STDPATH 外部污染、mktemp 隔离、trap 清场、无管道假 rc(sh 口径与在册坑位一致)。
- 全绿后:tests/COVERAGE.md 登记 + 单提交落库(pathspec 限定,勿卷对端在飞件)。

## 明确不做(v0 红线)

- payload 业务语义校验(事件类型注册表)、流式校验、多签/阈值、链头外部 anchor、PEM/Base64 密钥面——全挂 P2(契约 §6)。
