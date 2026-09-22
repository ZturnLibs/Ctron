# tests/db/replay_scram —— P5-D 夹具(SCRAM / 扩展查询 / 事务 / 取消)

**格式**:同 `../replay_fixtures/README.md` 定档(`*.script` 一行一帧十六进制,
`#` 注释/空行跳过/CR 剥除;server→client 下行帧,client 侧由 `std/db/pg.ct`
装配器构建并经十六进制字面量锚断言)。装载:`read_file` 内建 + 行切分,
`CT_DB_SCRAM` 环境变量指根(`tests/db/run.sh` 设定)。

## 录制来源登记

手工按 PostgreSQL 线协议 v3 公开规范构造(文档 §53;可复核落盘 =
`../replay_fixtures/gen_fixtures.py` 同一生成器,SCRAM 链 = python
hashlib 复算并对 RFC 7677 §3 原文常量自检,不一致即中止)。nightly 真库
导出钩子 Task 6 另标。

## SCRAM 向量改制口径(RFC 7677 §3)

- **emit 臂**(`x_scram_rfc7677.ct`):RFC 7677 原例逐字节(user/pencil,
  nonce `rOprNGfwEbeRWgbNEkqO`,盐 `W22ZaJ0SNY7soEsUEjb6gQ==`,i=4096,
  proof `dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ=`,
  v= `6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4=`)。
- **interp 臂**(`s_scram_hs.ct` 消费 `scram_ok.script`):同链低迭代改制
  (i=1;c=4096 ≈ 8192 块压缩,解释器堆不回收 P5-B 登记,interp 口径
  不可实用;实测 ≈1GB/块压缩 ⇒ 单链文件 ≈ 40GB 峰值已抵预算线)。
  改制只动 i,链几何/装配/逐字节锚不变(proof/v= 由生成器按 i=1 复算落盘)。

## 夹具清单与覆盖

| 文件 | 覆盖面 | 消费夹具 |
|---|---|---|
| `scram_ok.script` | SCRAM ok 全链:R10(机制表)→ R11(server-first)→ R12(v=)→ R0 → K → S → Z('I') | `s_scram_hs.ct`(双臂) |
| `scram_badpw.script` | 错口令路径:client-final 计出面 → server E(28P01)+ Z('E')(drain 面) | `x_scram_neg.ct`(emit) |
| `scram_badsig.script` | server-final v= 与本地 ServerSignature 不符 → clean err 4,sigok=0 | `x_scram_neg.ct`(emit) |
| `scram_nonce.script` | combined nonce 不以 client nonce 为前缀 → err 4(前缀校验在 PBKDF2 前) | `s_scram_hs.ct`(双臂) |
| `scram_plus.script` | 机制表仅 SCRAM-SHA-256-PLUS → err 4(channel-binding-plus 不支援,登记) | `s_scram_hs.ct`(双臂) |
| `scram_rfc7677.script` | RFC 7677 原例 i=4096 全链 | `x_scram_rfc7677.ct`(emit) |
| `ext_query.script` | 扩展查询响应流:1/2 跳过 + T + 2×D + C + Z(PQexecParams 形) | `s_ext.ct`(双臂) |
| `ext_describe.script` | 语句级 Describe:1 + t(参数 OID 表)+ T + Z(零行) | `s_ext.ct`(双臂) |
| `tx_cycle.script` | 事务状态迁移:Z('I')→Z('T')→Z('T')→Z('I'),游标链 | `s_tx.ct`(双臂) |
| `tx_error_drain.script` | E(25P02)收口 + 尾随 Z('E')排空 + ROLLBACK 复位见 Z('I') | `s_tx.ct`(双臂) |
| `tx_cancel.script` | 取消传播:行流中途 E(57014)+ Z('I'),排空后可复用 | `s_tx.ct`(双臂) |
| `tx_cancel_eof.script` | 流截断:源耗尽未见 Z → err 3 → dirty(1)弃用面 | `s_tx.ct`(双臂) |

## 口径注

- **interp/emit 分臂**(本目录 x_ 面):SCRAM 每条全链 = PBKDF2 + 证明 +
  签名 ≈ 23 块 SHA-256 压缩;解释器堆不回收(P5-B 登记)使 interp 单文件
  预算受内存限制(实测 c_pbkdf2_low ≈ 24GB / 单链文件 ≈ 40GB 绿,
  双链 ≈ 72GB 即 SIGKILL),故错口令/签名不符两走与 RFC 原例归 x_
  (结构性登记,emit 口径 < 1s)。
- **参数面 v0**:`t`(ParameterDescription)计数不与 Bind 参数核验(共驱核
  "其余跳过"前向兼容,登记);Bind 参数全文本格(format code 0 逐参),
  NULL = 长域 -1;result format codes 计数 0(全文本默认)。
