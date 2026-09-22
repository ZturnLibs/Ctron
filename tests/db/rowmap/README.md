# tests/db/rowmap —— 运行时行→struct 绑定夹具(P5-E;D-P5-1)

**夹具格式**:与 `tests/db/replay_fixtures` 同格式(PostgreSQL 线协议;一行一帧,
十六进制,`#` 注释)。`CT_DB_ROW` 环境变量指根,`tests/db/run.sh` 设定;
消费方 = `std/db/rowmap.ct`(经 `rm_query` 穿透;use 纪律见该文件头注)。

## 录制来源登记

手工按 PostgreSQL 线协议 v3 公开规范构造(行描述/数据行文本格;
`tests/db/replay_fixtures/gen_fixtures.py` 同法可复核)。

## 夹具清单

| 文件 | 覆盖面 | 消费锚 |
|---|---|---|
| `rm_rows.script` | 4 列混型行集:T(id int8 / name text / active bool / ratio float8)+ 4×D(row0 全非 NULL;row1 ratio NULL;row2 id = I64_MAX + ratio 科学计数 1e3;row3 id = 20 位十进制溢出)+ C + Z | `rm_anchors.ct` |

## 口径注

- **r7d_db_rowmap 锚语义修正(D-P5-1,本目录 born-correct)**:设计稿承诺
  "列缺失/类型不符**编译期**报"在现行语言(§8.4 禁泛型反射)下不可达,落
  **运行时 Err 面**——本目录锚按运行时 kind 断言(1 noent / 2 type /
  3 null / 4 row-oob / 5 val-domain),非编译期负例。COVERAGE 口径随
  Task 6 同步改写;`@derive(DbRow)` 插件 = 列编译泳道志向(v2 演进位,
  本文件 OID 表/列名匹配面即其运行时回落)。
- **NULL → Option**:kind 3 显式面,调用方 wrapper 映射(无隐式零值,§12.5)。
- 正例钉 = struct 字面量显式字段装配(零反射)。
- 本目录脚本文件为**规范消费面**——所有用例逐字节装载重放,非文档摆设。
