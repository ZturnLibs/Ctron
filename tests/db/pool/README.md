# tests/db/pool —— 有界连接池回放夹具(P5-E;§12.3 池纪律)

**夹具格式**:与 `tests/db/replay_fixtures`(PostgreSQL 线协议)同格式 ——
一行一帧,十六进制编码,`#` 注释;server 帧 `[type(1) | len(4 大端含自身) |
payload]`。装载经 `read_file` + 行切分(`CT_DB_POOL` 环境变量指根,
`tests/db/run.sh` 设定);消费方 = `std/db/pg.ct pg_query_script`(借出面
查询)+ `std/db/pool.ct`(回池复位路径)。

## 录制来源登记

手工按 PostgreSQL 线协议 v3 公开规范构造(`tests/db/replay_fixtures/
gen_fixtures.py` 同法可复核;锚值与 `replay_scram/tx_*.script` 系出同源)。

## 夹具清单与覆盖

| 文件 | 覆盖面 | 消费锚 |
|---|---|---|
| `p_tx_reset.script` | **Z('T') 陷阱**:C(BEGIN)+Z('T') 开事务;C(ROLLBACK)+Z('I') 复位路径 —— err 0 但 status 'T' 的连接必须复位方可回池(P5-D 评审裁定) | `pool_core.ct` |
| `p_reset_eof.script` | **复位失败**:事务开着而复位路径缺位(ROLLBACK at cur → 源耗尽)→ 弃用,永不回池 | `pool_core.ct` |
| `p_drain.script` | **drain 档**:E(25P02)留尾随 Z('E')→ 先排空再 ROLLBACK 见 'I'(Task 4 复位路径池侧接线) | `pool_core.ct` |
| `p_dirty.script` | **流截断脏标记**:T+D 后 EOF(err 3)→ dirty 弃用,不复位不回池(Global Constraints) | `pool_core.ct` |

## 口径注

- **等待接线(pool_wait.ct)**:Channel 等待面为本目录用例的接线钉(空闲槽
  通道容量 = 池容量;miss → recv 阻塞 = 背压显式;复位后 send 回池 = FIFO)。
  双臂可跑形态(while 闭包捕获;emit 臂 Sender/Receiver 不可作 struct 字段/
  形参 —— P5-E 探针实证并登记 divergences)。
- **槽位栅(fence)**:池帧库按连接拼接,复位消费以 `[fstart, fend)` 为栅,
  越栅 = 复位失败弃用(防串池)。
- 本目录脚本文件为**规范消费面**——所有用例逐字节装载重放,非文档摆设。
