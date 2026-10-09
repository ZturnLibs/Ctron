# http U8 平面化迁移计划（net 缓冲新视面收编）

> 上游：aa6fad0d「缓冲面 U8 字节平面化」点名遗留（ctecho 已迁，http 段候批）；
> 本批同时解锁：http 伞门 10 红、todo_api e2e（hashmap W2 换装运行时验收）、
> hashmap W2 收官闭环。

## 0. 勘察定界（2026-10-09 证据）

- **红集**（http 伞门）：client.ct inline、frm/serve.ct inline、client_fixtures/
  sse_ws/serve_e2e 五夹具——全部单根因 = `ctron_net_read_t/write` 外签已
  `var buf: U8[]`（ctron_view_w8u）而 http 侧仍是 `&I64[]`（ctron_view_6）。
- **enc/binddeflate 零耦合出局**：grep 全库无人调 deflate/gzip（自闭环面 +
  自有 C 垫片 ct_view6），整块不动。
- **otlp/trace/metrics/router 的 &I64[] 不在红集**：值数组（mx_hist counts/
  bounds）或自洽 pb 面不碰 net 外签，不迁移（编译器指路若牵入再随批）。
- **语言语义已探针实证（双臂绿）**：U8 元素读/写、字面量比较、U8→I64 算术
  宽化（acc*100+buf[i]）、byte_at；字节→Str 惯用法 = `byte_slice(字母表,
  c-32, c-31)` 查表（cx_slice_str 既有形）。
- 待批1探针补验：`&U8[]` 形参形态、U8[N] 字面量、byte_at 返回型对 U8 槽
  装载（直装 or as[U8]()）。

## 批1：parse.ct + message.ct + corpus 16 夹具

- 面：`&I64[]` 字节道形参 → `&U8[]`（parse 15 处/message 4 处，纯层无外签）；
  内部字节读零改动（宽化已证）；corpus 每件 `feed()` 助手
  `buf[i] = byte_at(s,i).as[I64]()` → U8 直装（或 as[U8]()）。
- 门：http/run.sh 的 parse/message inline 双臂 + corpus 16 件双臂全绿
  （e2e 段仍红 = 批2 前预期）。
- 提交：feat(http) 单批。

## 批2：client.ct + sse.ct + ws.ct + frm 耦合闭包 + 四 e2e 夹具

- 面：client（extern 直呼 6 处 + client_read_t/write 面 + body/buf/scr）；
  serve（sv 循环）；sse/ws 编解码 lane（夹具 buf 换 U8 后随之）；
  frm/connect（req 透传）/middleware（frm_ctx_param）——router/trace/otlp/
  metrics 以编译器指路为准（红才迁）。
- 夹具：x_client_e2e/x_sse_e2e/x_ws_e2e/x_serve 的 I64[N] 缓冲与 fx 助手
  （fx_fill/fx_eq_bytes/fx_has 等）随批 U8 化。
- 门：tests/connect c0 双臂 + http/run.sh 伞门全绿（除 env-gated skip）。
- 提交：feat(http) 单批。

## 批3：todo_api + 全局收官

- todo_api：buf/scr I64[4096/2048] → U8[]，respond/读环随批；run.sh e2e
  22 断言全绿（hashmap W2 换装的运行时验收同步兑现）。
- 门：todo_api run.sh + smoke 全量（decls 锁视对端 W2.5 落地态实测处理）+
  suite 防御 + tests/net 冒烟（net 门不动应不受扰）。
- 收官：COVERAGE U8 批注 + spec W2 行「候补跑」翻转 + 记忆 + 报告。

## 纪律

机刷 pathspec 限定/零 compiler/src/不 stash；每片全绿单独提交；对端在飞
（gui_parse/s96）文件不碰。
