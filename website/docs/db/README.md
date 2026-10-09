# db 域包 —— 数据访问

pg / redis 线协议 + 连接池 + rowmap 的单域集合。**消费形态 =
`use db.<子模块>.{...}`**;规范面 = [语言规范·数据访问](../spec/12-db.md)。

## 布局

```
db/db.ct      门面:能力 token + DSN 解析纯面(参考页 db.md)
db/pg.ct      PostgreSQL 线协议(参考页 pg.md)
db/redis.ct   RESP 线协议(参考页 redis.md)
db/pool.ct    连接池(参考页 pool.md)
db/rowmap.ct  行映射(参考页 rowmap.md)
db/c_src/     C 胶水
```

## DSN 文法(v0)

```
postgres://user:pass@host:port/dbname   (postgresql:// 前缀同收)
```

port 缺省 5432;user 必填(空 = ok 0);pass/name 可空;百分号编码与
IPv6 `[::1]` 字面量 v0 不支援(畸形 → ok=0 全空,不抛)。

## 口径与坑位(写码前必读)

1. **能力面**:门面函数首参 `db: StdDb` 贯穿(`db_std()` 构造,`db.connect`
   键的调用点显形;net/tls 惯例同款);`#[pure]` 触库 = E4020 编译期拦截。
2. **零 std 依赖纯叶**:db 不 use std.net——TCP 连接由消费侧组合
   (net 树建连 → 句柄 fd 传 pg 真源;严格互斥树,跨包引用即请求)。
3. **发射面 struct 通道**:跨模块数值消费走 getter(`pgd_ok/pgd_user/...`),
   不暴露字段直读(PgRows 同款)。
4. inline test 不落 db 包文件(C17 宿主敏感);行为面在 `tests/db/`
   (corpus/pool/redis_replay/nightly)。

验收:`tests/db/` 全族 + nightly 冒烟;域包消费测试 headless 绿为准入。
