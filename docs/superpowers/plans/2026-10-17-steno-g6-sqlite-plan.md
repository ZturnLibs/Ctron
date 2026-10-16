# G6 SQLite 绑定 实施计划(vendored amalgamation + sqlite.ct 门面)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** P1-7 落地——本地库绑定:SQLite 3.54.0 vendored amalgamation 走 c_src 同批(J5 已裁),`lib/db/sqlite.ct` 门面 MVP 面 = open/prepare/step/bind 标量四族/column 取值/errmsg;事务 v0 = `exec("BEGIN")`。SQLCipher = 换 amalgamation + pragma key 同一张门面(P2 挂账)。

**Architecture:** 单缝(库面)零编译器面。垫片微形:仅 open/prepare 两个 C 出参翻返回值(静态存 rc;pg `ctron_dbpg_connect` 直返 fd 同形),其余 step/finalize/bind/column/exec/errmsg/changes/rowid/close 全直宣 extern(bind.ct 单行纪律)。能力 token 自有 `SqliteDb`(对齐 db.ct:18-25 StdDb 形,不 use db.ct——严格互斥树)。

**Tech Stack:** sqlite-amalgamation-3540000(sqlite.org;sqlite3.c 9.6MB);str_from_c 语言内建(深拷 C-owned 串,§9.6);emit-only 门(tests/db/sqlite/run.sh,interp 无 extern 运行时)。

## Global Constraints

- 家法扁平淡入:**`lib/db/sqlite.ct` + `lib/db/c_src/`**(设计档 §8 的 `lib/db/sqlite/` 子目录形为解析链未验证形,MVP 不冒险;偏差登记)。
- 跨模块数值消费 = struct 标量字段 + 自由 getter 家法(HttpOpen/open_* G5-W3 同款)。
- extern 单行形参表(C 宿主解析器);句柄全程 I64;`Str → const char*` 零拷直通。
- pathspec 限定提交;每 Wave 独立绿独立落库。

---

### Wave 1: vendored amalgamation 落位 + sqlite.ct 门面 MVP 面

- [x] `lib/db/c_src/sqlite3.c` + `sqlite3.h`(3.54.0 原样落位;shell.c/sqlite3ext.h 不取)+ `lib/db/c_src/ctron_sqlite.c` 微垫片(open/prepare 出参→返回值,rc 静态槽)
- [x] `lib/db/sqlite.ct`:能力 token `SqliteDb`/`sqlite_std()`;`SqlConn{ok,h}`/`SqlStmt{ok,st}` + getter 族;open/prepare(走垫片)/step/finalize/bind_i64/f64/text/null(TRANSIENT=-1 直绑)/col_count/type/i64/f64/text(str_from_c 深拷,NULL→空)/errmsg/errcode/exec(五参三零)/last_rowid/changes/close
- [x] decl smoke(interp 臂 `ctron-cc run` 两件:门面本件 + use 消费件)
- [x] Commit

### Wave 2: tests/db/sqlite 门 + 双跑绿 + 批终

- [x] `tests/db/sqlite/run.sh`:emit-only 门(sqlite3.o 一次编译 `-DSQLITE_OMIT_LOAD_EXTENSION`[省 -ldl],夹具只链 .o;uname 分支 Linux -lpthread);decl smoke 入 interp 臂
- [x] 夹具七件:①内存库 DDL+CRUD 往返(int/text/f64/null 四族)②bind 复用(reset+重绑)③errmsg 面(坏 SQL → rc≠0 + 消息非空)④last_rowid/changes⑤文件库落盘重开持久⑥多语句 exec 脚本(BEGIN/INSERT/COMMIT)⑦col_type 判别 + NULL 面;探针行 + grep 断言(tests/down 家法)
- [x] 双跑稳定 + tests/db 基线不红(56/56 族不触)+ Commit + 批终登记(design/计划/memory)

## 明确不做

- blob 双向(bind_blob/column_blob 需 U8[]↔void* 整形,非 MVP 四族;挂账);SQLCipher/OpenSSL(P2);大 .a 预编译缓存(P2 vendoring 约定);dlopen/#[link] 系统库形态(J5 已裁 vendored);多线程连接共享面(v0 单连接单线程假设成文;FULLMUTEX 挂账)。
