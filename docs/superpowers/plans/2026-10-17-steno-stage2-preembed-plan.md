# Stage 2 预埋计划 —— engine-proxy 基准族 + soak 门(Ctron 侧)

- 状态:执行中(2026-10-17;Stage 0 落库后秘本泳道剩余的唯一纯 Ctron 侧批)。
- 依据:steno-gap-fill-design.md §11「秘本验收三线的 Ctron 侧落法」——RTF 线 = tests/bench 新族 engine-proxy(PCM 分块循环 + FFI 桩忙循环,digest pin + ×3 min 惯例,真 sherpa 对拍留秘本仓);内存线 = RSS pin soak;零崩线 = soak rc=0 + 无 panic。Stage 2 正式 A/B(Rust 版对照)待秘本仓存在后进行,本批只预埋 Ctron 侧机器与基线。

## 交付物

| 件 | 位 | 内容 |
|---|---|---|
| engine-proxy 基准 | tests/bench/engine-proxy/ | 100 会话 × 10 段 × 20 帧 × 256 样本(16kHz 模型 = 320s 音频);逐帧 LCG 样本 + 定值算术核 + `sv_stub_work` FFI 忙循环;**顺序形**(与 C 参照同构可比);digest 逐字节钉 vs ref.c + ×3 取 min + 比值门 ≤1.2(RTF 线同数);RTF 自报(mono_ns) |
| C 参照 | 同目录 src/ref.c | 同工作量同 stub 同输出格式(C 笔算核 = 引擎原生效率下界代理) |
| FFI 桩 | 同目录 c_src/stub.c | `sv_stub_work(iters)` volatile 忙循环,两侧同链同 -O1(周期等效) |
| soak 门 | tests/steno/soak/ | TaskPool(8) × 100 会话 × 20 段(2000 任务,池化有界并发)+ Channel 收口逐位求和(顺序确定 digest)+ rc=0 + 无 panic 文案 + max RSS pin(/usr/bin/time -l,界=标定值 ×2)+ CTRON_GC=off/coro 双臂 |

## 关键裁决

1. **bench 顺序形 / soak 并发形分置**:比值门要同构可比(C 参照不做 pthread),并发面归 soak——两门合起来才覆盖 §11 三线。
2. **并发形 = TaskPool + Channel 收口**(pool/joinall 夹具正典):池无 wait 面,完成 = 逐 recv 计数;scope join-all 只作兜底语义(不依赖);spawn 句柄不出 scope 型别即断(探针实证,`let t = scope{…}` 出块即 int32 化)。
3. **digest 无锁确定**:逐任务一 I64 过通道、主顺序求和——无 Atomic 求和的序自由面,摘要天然逐字节钉。
4. **无位运算血律**:LCG/算术核全算术等价(I64 乘加 + mod 小素数);样本域钳 I16 幅度。
5. **RSS 口径**:macOS `/usr/bin/time -l` maximum resident set size(字节);界 = 标定实测 ×2 写死并注值,非理论推基。

## 验收门

- `ctron bench engine-proxy`(注册表 +1 行)→ digest pin 绿 + 比值打印(≤1.2 PASS/WARN 档,lang/bench 同制)。
- `sh tests/steno/soak/run.sh` → 双臂各 rc=0 + `soak-probe done tasks=2000 digest=<钉值>` + RSS < 界。
- 全绿后 COVERAGE.md 登记 + pathspec 限定落库(对端在飞,禁 add -A)。

## 明确不做

- 真 sherpa/onnxruntime 对拍(秘本仓 A/B);Rust 版 RTF 对照(秘本仓不存在)。
- benchmark 时值 pin(只钉 digest;时值漂移归因靠 min-of-3 报告,不入门禁)。
- 多进程/多机 soak;断点续跑 soak(Stage 2 正式批面)。
