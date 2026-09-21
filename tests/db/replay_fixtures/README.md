# tests/db/replay_fixtures —— PostgreSQL 线协议回放夹具(P5-C)

**录制格式(VENDORED 式定档;§12.7 协议夹具回放,CI 零真库)**

## 脚本文件(*.script)

- 一行一帧,**十六进制编码**(小写规范,大写数位解码兼容),字节序 = 线上字节序。
- server 帧:`[type(1) | len(4, 大端, 含自身 4 字节) | payload]`;client 帧
  startup 无 type 字节,`Q`/`P`/`X` 有。本波脚本只承载 **server→client** 帧
  (下行);client 侧消息由 `std/db/pg.ct` 装配器构建,经十六进制字面量锚
  断言(`tests/db/corpus/a_builders.ct`)。
- 注释:行首 `#`;空行跳过;CRLF 的 CR 剥除。
- 消费方:`std/db/pg.ct` `pg_recv_frame_script(frames, cur)` —— `List[Str]`
  逐帧出(夹具经 `read_file` 内建装载 + 行切分,`CT_DB_FIX` 环境变量指根,
  `tests/db/run.sh` 设定)。核心校验:声明长域 [4, 1048576]、声明长 ==
  总长-1(逐帧完整前提)、十六进制合法性;违例 → clean Err(err 码表见
  pg.ct 头注),无崩溃无挂起。

## 录制来源登记(计划 Task 3 步骤②)

本波夹具来源 = **手工按 PostgreSQL 线协议 v3 公开规范构造**(文档 §53
Frontend/Backend Protocol);构造过程可复核落盘 = 同目录
`gen_fixtures.py`(锚生成器纪律,P5-B 同款,幂等重写)。
**nightly 真库导出钩子 Task 6 另标**(`tests/db/nightly/`:真 Postgres
连录导出为本格式,本地无真库则 SKIP 登记口径)。

## 夹具清单与覆盖

| 文件 | 覆盖面 | 消费夹具 |
|---|---|---|
| `startup_ok.script` | 握手 ok 路:R(AuthOk)+ 2×S + K(pid/key)+ Z('I') | `b_handshake.ct` |
| `query_rows.script` | 简单查询全行集:3 行 × int4/text/bool 混型,全非 NULL | `c_query_rows.ct` |
| `error_response.script` | 错误响应面:SQLSTATE 42P01 传播(S/V/C/M/P 字段扫描) | `d_error_surface.ct` |
| `bad_length.script` | 协议错帧拒收:长度/实际不符、域坏、非十六进制、奇长 → clean Err | `e_protocol_violation.ct` |
| `empty_result.script` | 空结果集:T 在、零 D、tag "SELECT 0" | `c_query_rows.ct` |
| `null_value.script` | NULL 值:列级 −1 长;同行非 NULL 对照 | `f_null_utf8.ct` |
| `utf8_bytes.script` | 多字节 UTF-8 格:字节精确面(hex/bytes);ASCII 面 fail-closed 锚 | `f_null_utf8.ct` |
| `auth_stub.script` | 认证存根检测:Cleartext(3)/SASL(10)→ err 4(SCRAM = Task 4) | `b_handshake.ct` |

## 口径注

- **帧长域 [4, 1048576]**:2^20 上界远低 (i) 族 2^28 险带(乘前高位预门 +
  宽域播种,C10);真库大行(>1MiB 单帧)Task 6 nightly 按需再议。
- **utf8/ASCII 解码面**:解释器 `utf8_enc` 恒返 U+FFFD(发射面正确;
  divergences P5-C 登记)⇒ 格文本解码走可打印 ASCII 表切片(非打印/
  多字节 fail-closed = 空串),任意字节精确面由 `pgr_cell_hex`/
  `pgr_cell_bytes`(List[I32] 字节容器)承载;解释器修复后 ASCII 面收敛
  为全文本面,夹具字节锚不变。
- 本目录脚本文件为**规范消费面**——所有 corpus 夹具逐字节装载重放
  ("解析器对录制字节逐字节正确"口径),非文档摆设。
