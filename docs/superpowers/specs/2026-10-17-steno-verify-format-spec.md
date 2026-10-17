# steno-verify 格式契约(stenolog.v1 / stenolicense.v1)

- 状态:**Stage 0 契约定案**(本文即「秘本侧计划可见」前置件的落法:审计链与 license 的字节格式、验证语义、对拍协议自此有据可依;秘本引擎 Rust 侧落 steno-store/steno-license 时按本文生产,双侧以本文为唯一契约源)。
- 日期:2026-10-17。
- 上游:steno-cross-platform-design.md v0.2(D4/D5:审计链=事件哈希链+ed25519,license=ed25519 离线验签);ctron-gap-roadmap.md §4 Stage 0;steno-gap-fill-design.md §11。
- 消费方:Ctron 侧 `steno/verify/`(steno-verify 工具,Stage 0 dogfood 交付物);秘本侧 steno-store(生产者)。

## 0. 一句话

审计链 = 逐事件 JSONL,每行自持「SHA-256 哈希链环 + Ed25519 签名」,验证器对**生产者写出的字节**逐字复算,零再序列化歧义;license = 单行 JSON 信封,同款签名面。

## 1. stenolog.v1 —— 审计链格式

### 1.1 文件形态

- UTF-8 文本,LF 行尾(容忍行尾 `\r` 剥除),每行一个事件,**行序即链序**。
- `seq` 从 0 起连续递增,行下标 i 处 seq 必须 == i,不得跳号。
- 空文件(零事件)= 合法空链,验证通过。
- 末行后允许一个换行;文件中段空行 = 非法。
- 禁 BOM。

### 1.2 事件行(生产者强制:紧凑 JSON、固定字段序)

```
{"v":1,"seq":S,"ts":T,"kind":"K","actor":"A","payload":P,"prev":"R","hash":"H","sig":"G"}
```

| 字段 | 型 | 约束 |
|---|---|---|
| v | 数值 | 恒 `1`(格式版本) |
| seq | 数值 | 非负整数;== 行下标;创世行 = 0 |
| ts | 数值 | Unix 毫秒时间戳,非负整数 |
| kind | 串 | 非空,`[A-Za-z0-9_.-]{1,64}`(import/transcribe/edit/export/activate…) |
| actor | 串 | 生产者身份,可空串 |
| payload | 任意 JSON 值 | 语义自由(v1 建议对象);验证器按不透明字节对待 |
| prev | 串 | 64 位小写 hex;创世 = `"0"×64`;否则 = 上一行 `hash` |
| hash | 串 | 64 位小写 hex;= SHA-256(core) |
| sig | 串 | 128 位小写 hex;= Ed25519(priv, core 字节);Base64 不用,全线 hex |

### 1.3 core 提取规则(本契约的核心裁决)

**core = 行内最后一个 `,"hash":"` 标记之前的全部字节**(即 `{"v":1,...,"payload":P,"prev":"R"`——以 prev 值的收引号结尾,不含 hash/sig/尾花括号)。

- 无歧义性:合法 JSON 行内,字符串值内部的 `"` 必为 `\"` 转义,故裸字节 `,"hash":"` 只能作为结构出现;取「最后一处」后,(payload 内的对抗内容同样无法伪造第二处未转义标记)。
- 由此 **hash 与 sig 覆盖生产者写出的精确字节**:验证器对 core 逐字复算 SHA-256 与 Ed25519,**不做任何 JSON 再序列化/键序规范化**——生产者与验证器之间的唯一约定就是固定字段序 + 紧凑(无空白)。
- 附带校验:行内 `,"hash":"` 与 `,"sig":"` 各恰一处;行以 `"<64hex>"}` 收尾(sig 值后恒为收引号+右花括号);`,"hash":"` 之后恰 64 字节 hex + 收引号一位,再接 `,"sig":"`(间距不变量:sm == hm + 9 + 64 + 1)。

### 1.4 逐行验证步骤

1. 行非空;JSON 可解析;顶层对象;§1.2 全字段型与约束核对。
2. core 提取;`SHA-256(core) == hash`。
3. 链环:i==0 时 `prev == "0"×64`;否则 `prev == 上一行 hash`。
4. `Ed25519_verify(pub, core, sig) == 1`。

任一步失败即整链无效,报 `line <i>: <原因>`(首个失败即停)。

### 1.5 验证结果与退出码

- 成功:`OK stenolog.v1: <n> events, chain intact, signatures valid`(另报末行 hash 作链头锚);`--json` 出 `{"format":"stenolog.v1","valid":true,"events":N,"head":"H"}`。
- 失败:退出码 1;`--json` 出 `{"format":"stenolog.v1","valid":false,"line":I,"reason":"…"}`。
- 用法/IO 错误:退出码 2。

## 2. stenolicense.v1 —— license 文件格式

### 2.1 信封(单行紧凑 JSON,固定字段序)

```
{"v":1,"product":"steno","email":"E","machine":"M","issued_at":T1,"updates_until":T2,"nonce":"N","sig":"G"}
```

| 字段 | 型 | 约束 |
|---|---|---|
| v | 数值 | 恒 1 |
| product | 串 | 非空 |
| email | 串 | 非空(购买者标识) |
| machine | 串 | 64 位小写 hex(机器指纹 = 平台指纹的 SHA-256,验证器不解释) |
| issued_at | 数值 | Unix 毫秒,非负 |
| updates_until | 数值 | Unix 毫秒,非负(更新期截止;=0 表示永久) |
| nonce | 串 | 32 位小写 hex(防重放放发面,v1 不强制唯一性核对) |
| sig | 串 | 128 位小写 hex;Ed25519(priv, core) |

**core = 行内最后一个 `,"sig":"` 之前的全部字节**(以 nonce 收引号结尾)。

### 2.2 验证语义

- **真伪(base)**:§1.4 步骤 1/2/4 的同款核对(链环步不适用)。通过 = 退出码 0,license 为真。
- **机器绑定**:`--machine <hex>` 给出时与 `machine` 字段比对,不合 = 无效(退出码 1)。
- **新鲜度**:`--now <毫秒>` 给出时报 `updates_expired` 布尔(`updates_until != 0 && updates_until < now`);`--strict` 时 expired 亦判无效(退出码 1)。缺省只报告不判死。
- `--json` 信封:`{"format":"stenolicense.v1","valid":B,"product":…,"email":…,"machine":…,"updates_expired":B?}`。

## 3. 密钥面

- Ed25519,32 字节裸钥,**hex 文本交换**(私钥 64 字符,公钥 64 字符),无 PEM/Base64。
- `--pub`/`--priv` 取值:`<hex>` 直给,或 `@<path>`(文件内一行 hex,剥尾白)。
- 私钥文件落盘 0600/O_EXCL(工具 keygen 子命令负责;消费方自管同规)。

## 4. 工具面(steno-verify 单二进制)

```
steno-verify keygen --out <priv 文件路径>        # 新密钥对;私钥写盘 0600,公钥打 stdout
steno-verify chain --pub <hex|@file> <audit.jsonl> [--json] [--quiet]
steno-verify sign-chain --priv <hex|@file> <in.jsonl> <out.jsonl>
                 # in = 未签名事件行(§1.2 前 6 字段,无 prev/hash/sig)
                 # out = 补链环与签名后的完整 §1.2 行
steno-verify license --pub <hex|@file> <license.json> [--machine H] [--now MS] [--strict] [--json]
steno-verify sign-license --priv <hex|@file> <in.json> <out.json>
steno-verify selftest                            # RFC 向量 + roundtrip + 篡改负例
```

## 5. 独立对拍协议(验收裁判)

验证器的「独立」性以 OpenSSL 为第二实现裁判,双向:

- **A 向(我签他验)**:OpenSSL 生成 ed25519 裸钥 → 提取 priv/pub 裸 hex → `sign-chain` 以 priv 签链 → 取任一 core 字节,`openssl pkeyutl -verify -rawin -pubin` 验之,必过。
- **B 向(他签我验)**:同一 OpenSSL 私钥对同 core 签名(Ed25519 确定性,同钥同文必同签)→ 与我方 sig 逐字节比对,必等;`chain` 以对应 pub 验全链,必过。
- 负例矩阵:链环断裂(prev 篡改)/ payload 篡改(hash 失)/ sig 篡改 / seq 跳号 / 创世 prev 非零 / 字段缺失 / 非法 JSON——各必拒。

## 6. 边界与在册

- 全文读入(非流式)为 v0 口径;审计文件流式校验挂 P2(随秘本侧真量级出现再立)。
- payload 的语义校验(事件类型注册表)不在验证器职责内——验证器只证**完整性与真伪**,不证**业务合规**。
- 多签/阈值签名、链头定期 anchor(外部时间戳)挂 P2。
