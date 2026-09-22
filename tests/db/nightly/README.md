# tests/db/nightly —— 真靶冒烟(P5-F;Task 6;不入 CI 主环)

口径:tests/db 主环(tests/db/run.sh)全夹具回放零真库;本目录 = 真
Postgres / Redis 连接冒烟,nightly 或本机有真靶时跑,无则 **SKIP 登记**
(逐行输出,诚实记录,不虚构结果)。

- 靶源:env 驱动 `CTRON_PG_DSN`(postgres://user:pass@host:port/db)
  / `CTRON_REDIS_URL`(redis://host:port);未设则探本机常见口
  (PG 5432 / Redis 6379;`nc -z` 探活);探不达 → SKIP。
- PG 面:connect(垫片真源)→ startup → **SCRAM-SHA-256 全链**
  (pg_scram_handshake_fd;口令认证必需——trust 认证与 plus-only 按
  RFC 5802 客户端 MUST 拒收,err 4 显形)→ 简单查询 → 预编译
  (pg_ext_pipeline)→ 事务(BEGIN/COMMIT 状态字节 I→T→I)→ Terminate。
- Redis 面:connect → SET → GET 回环 → INCR ×3 计数 → DEL。
- 构建面:run.sh 先 build(emit + cc 链自有垫片)再执行——无靶时
  构建门照跑(fixtures 健康检查),执行段 SKIP。
- 会话组合 = db.ct 头注登记的"消费侧组合"形态:nightly 自组 fd 会话
  (net 建连 → 句柄 fd 传驱动),不拉 std.net 进 std/db 树。
