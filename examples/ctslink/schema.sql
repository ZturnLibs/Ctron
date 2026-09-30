-- ctslink PG schema(设计稿 §二;nightly/真跑手工应用——run.sh 与 serve_net.ct
-- 原生臂 env CTRON_PG_DSN 时亦按此 DDL 建表(幂等 CREATE TABLE IF NOT EXISTS)。
-- 语义门(test_call)不依赖 PG:应用层走内存 store 变体(README「存储双源」口径),
-- 本 DDL 与内存行一一对应:users.id/links.owner 即内存 User.id/Link.owner。

CREATE TABLE IF NOT EXISTS users (
    id      BIGINT PRIMARY KEY,
    name    TEXT UNIQUE NOT NULL,
    pwhash  TEXT NOT NULL,          -- pbkdf2$<iters>$<salt_hex>$<hash_hex>
    created BIGINT NOT NULL         -- unix 秒
);

CREATE TABLE IF NOT EXISTS links (
    id      BIGINT PRIMARY KEY,
    owner   BIGINT NOT NULL,        -- users.id
    code    TEXT UNIQUE NOT NULL,   -- 7 位 [a-z0-9]
    url     TEXT NOT NULL,          -- http/https 白名单
    created BIGINT NOT NULL,
    deleted BOOLEAN NOT NULL DEFAULT FALSE
);

-- 点击计数不在 PG:Redis 单连 sl:clicks:<code> INCR(失败不拦跳转,v1)。
