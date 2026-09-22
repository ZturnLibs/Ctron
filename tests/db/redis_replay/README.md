# tests/db/redis_replay —— Redis RESP2 回放夹具(P5-E)

**录制格式(VENDORED 式定档;§12.7 协议夹具回放,CI 零真库)**

## 脚本文件(*.script)

- 一行一条**完整回包**,十六进制编码(小写规范,大写数位解码兼容),字节序 =
  线上字节序(含 RESP 行终止 CRLF 的 0d0a 字节)。
- 与 pg 夹具的帧长前缀不同:RESP2 回包**自终止**(CRLF / 长度域自述),故一行
  即一整回包,无外置长度域;多行 = 多回包(如 `r_incr_seq.script` 计数序列)。
- 注释:行首 `#`;空行跳过;CRLF 剥除(loader 同 pg 夹具)。
- 消费方:`std/db/redis.ct` `redis_recv_script(lines, cur)`(`read_file` 装载 +
  行切分,`CT_DB_REDIS` 环境变量指根,`tests/db/run.sh` 设定)。校验:五形
  (+/-/:/$/*)形式面、bulk 长域 [0, 512MiB](乘前界门,C10)、截断/嵌套数组
  拒收;违例 → clean Err(err 码表见 redis.ct 头注),无崩溃无挂起。

## 录制来源登记(计划 Task 5)

本波夹具来源 = **手工按 Redis RESP2 公开协议规范构造**(RESP2 序列化文档;
回包字节 = 锚生成器同法以 python 逐字节展开落盘,幂等可复核)。**nightly 真库
导出钩子 Task 6 另标**(`tests/db/nightly/`:真 Redis 连录导出为本格式,本地无
真库则 SKIP 登记口径)。客户端命令装配侧锚 = 测试内十六进制字面量
(`rr_replay.ct` encoder anchors)双向钉死(装配器 ↔ 解析器对偶)。

## 夹具清单与覆盖

| 文件 | 覆盖面 | 消费锚 |
|---|---|---|
| `r_get_ok.script` | bulk string 命中($5)+ bval 精确字节面 | `rr_replay.ct` |
| `r_get_nil.script` | NULL bulk($-1)→ isnull | `rr_replay.ct` |
| `r_set_ok.script` | 简单字符串(+OK) | `rr_replay.ct` |
| `r_error.script` | 错误回包(-ERR)→ rc -1 err 4 原文面 | `rr_replay.ct` |
| `r_wrongtype.script` | 错误回包第二形(-WRONGTYPE;INCR 型错) | `rr_replay.ct` |
| `r_incr_seq.script` | 整数形(:)×3 计数序列(游标链) | `rr_replay.ct` |
| `r_del_expire.script` | DEL miss :0 / EXPIRE ok :1(整数形负值/零) | `rr_replay.ct` |
| `r_array.script` | 数组形(*2;一层数元展开) | `rr_replay.ct` |
| `r_array_null.script` | 数组内 NULL 元($-1 中置) | `rr_replay.ct` |
| `r_empty_bulk.script` | 零长 bulk($0) | `rr_replay.ct` |
| `r_badform.script` | 未知类型字节 → err 2 | `rr_replay.ct` |
| `r_hugebulk.script` | bulk 长域越 512MiB 界 → err 2(C10 乘前门) | `rr_replay.ct` |
| `r_truncbulk.script` | bulk 声明长 > 实际字节(截断)→ err 2 | `rr_replay.ct` |

(非脚本夹具:`x_rd_fd.ct` = fd 真源面 emit 臂专面 —— 半包续读(分片 bulk)/
NULL bulk / eof 三钉 + send-all 命令整发;链 `std/db/c_src/ctron_dbredis.c`。)

## 口径注

- **bulk 长域 [0, 512MiB]**:RESP 协议上限 512MB;逐位累计**乘前预门**
  (>536870912 即拒,恒 I64 宽域)——512MiB 已过 2^28,不做一次性宽乘
  ((i) 族 Global Constraints)。
- **ASCII/字节面**:文本解码走可打印 ASCII 表切片(utf8_enc 解释器坏面,
  pg.ct 头注③同登记);bulk 精确字节面 = `rs_bval`(List[I32])+
  `redis_hex_encode`(任意字节双臂可断言)。
- **数组形**:一层数元单展开(GET/SET/DEL/EXPIRE/INCR 基本面不产嵌套;
  嵌套数组 = err 2,v0 登记口径)。
- 本目录脚本文件为**规范消费面**——所有用例逐字节装载重放,非文档摆设。
