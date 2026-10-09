# std/hashmap W2 场景替换计划（http keep-alive 多槽 + todo_api 存储）

> 上游：spec `2026-10-09-std-hashmap-design.md` §9 W2 行；W1 已落库 bfe22c77。
> 每片全绿即单独提交；机刷纪律（pathspec 限定/零 compiler/src/不 stash）沿用。

## 0. 价值重估（2026-10-09 实施 survey 后修订）

| 原靶点 | 裁决 | 理由 |
|---|---|---|
| http client keep-alive 多槽 | **做（Slice A，旗舰）** | 唯一登记在册的"已证好形"绕行（四标量胞单槽，client.ct ④ 面）；W1 探针已退役其登记根因（Box 字段 StructLit 出口径 → 今发射绿） |
| todo_api 平行 List | **做（Slice B）** | 注释自认绕"List[I64] 元素读发射债"；顺带实证 HMapI V=struct 组合 |
| web headers List[Pair]→HMap | **不做** | HTTP 头语义 = 有序且可重名（Set-Cookie），List[Pair] 是正确模型；首-match 查找 O(n) 头 20 条无痛点。设计文档 W2 行同步回写 |

## Slice A：http client keep-alive 多槽

**拦路虎与形**：client.ct use std.hashmap 后，消费方若直用 std.hashmap 即 E5020
互斥树中毒（client.ct 头注 (g) 加码实测）⇒ 缓存载体做进 client.ct 内**不透明
结构**，消费方零新 use：

```ctron
pub struct HttpKeep {
    var cache: Box[HMapS[I64]]   // "host:port" → fd
    var bru: Box[Box64]          // 复用计数(测试可见,语义不变)
}
pub fn http_keep_new() -> HttpKeep
```

**面变更**：
1. `client_request` 尾四参（bfd/bhh/bpt/bru）→ `kp: HttpKeep` 单参。
2. `HttpResp` 加 `let fd: I64`（初值 -1）+ `rr_fd` getter——101 直通契约改经
   resp.fd 交付升级连接（原经 bfd.v；101 时缓存 del 键位 + resp.fd = fd）。
3. 缓存语义（其余逐字保持：复用败重试一次、-2 重试、部分响应不重发、上限 5 跳）：
   - 键 = `host + ":" + port.to_string()`；命中 → fd = hms_get，reused = 1；
   - 未命中且 len ≥ 8 → FIFO 逐出（close + del 首键，hms_keys[0]）再连；
   - 跨 host 切换**不再关旧连**（多槽本旨）；发送败/读 -2 复用败 → close + del + 重试；
   - keep = 0 → close + del；101 → del 键位 + resp.fd = fd；
4. `client_hash_host` 保留为遗面（inline tests 钉确定性；缓存改键符不再用）。
5. 头注 ④ 面重写（根因退役记录：Box 字段 StructLit/跨模块 Box 通道已随 W1 探针
   B1–B4 翻绿；E5020 毒图 = HttpKeep 不透明形之因）。

**消费者（全量 survey）**：`lib/http/frm/connect.ct`（connect_unary 四胞透传 →
kp 单参，注释同步）、`tests/connect/send.ct`、`tests/http/sse_ws/x_sse_e2e.ct`、
`tests/http/sse_ws/x_ws_e2e.ct`（含 101 段则走 resp.fd）、tests/http/bench +
tests/net/bench 持胞点（实施时 grep `bfd` 全量补扫）。

**门**：`sh tests/http/run.sh`（client inline tests 走 emit 专臂）、
`sh tests/connect/run.sh`、`sh tests/net/run.sh`（冒烟子集）、
`sh compiler/test/smoke.sh --full`（回归基线 236/0）。

## Slice B：todo_api 存储 HMapI

- 平行三 `List[Str]`（ids/titles/dones，注释自认绕发射债）→
  `var store = hmi_new[Todo]()`（Todo = 值结构，形态随现有 JSON 序列化代码定）+
  next_id 序列；CRUD/列表按 id 走 hmi_get/hmi_put/hmi_del；列表序 = 插入序
  （hmi_vals）。
- run.sh `CTRON_STDPATH=lib/std` 直指主库 ⇒ 零快照工作，use std.hashmap 即可。
- 门：`sh examples/todo_api/run.sh`（22 断言 e2e，发射臂）。

## 非目标

web headers/路由表不动（路由 = 模式匹配非精确键，哈希不适用）；Slice A 不动
sse/ws 纯叶（use 图互斥树不涉）；不做 LRU touch（FIFO 文档化，v1 YAGNI）。
