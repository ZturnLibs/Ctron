# T50/S3-α 黄金轨迹协议(纯函数录制/复放)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地闭源分发信任协议(D3「确定性重放取代信任发布者」)的最小真切片:对导出纯函数录制黄金轨迹(值 = std json 规范形),复放比对,不符即拒——录/放两侧逐字确定,轨迹入工件即被 SHA256SUMS/self_digest 覆盖(防篡改免费继承)。

**Architecture:** 录制 = 编排层代码生成:每用例生成一行 `println(write_json(<fn>(<args 字面量>)))` 的驱动 .ct,`bin/ctron-cc run` 跑(source 模式,exe 旁 std 解析 ✓),stdout 即规范形期望值。轨迹文件 = `traces/<stem>.ctrt`(CTCL 键控块:`trace "<fn>" { args = "<字面量>"` + `expect = "<json>"` `}`)。复放 = 同码gen 重跑 + 逐字比对。工件面:seal 编排把 `traces/**` 并入 SHA256SUMS(一行 find 扩),self_digest 随之覆盖轨迹字节。效果函数(FakeFs 脚本化录制)与加载期 E4040 拒载 = S3-β,本计划不做并如实登记。

**Tech Stack:** shell 编排(ctc.sh/新 tools 面)、bin/ctron-cc(source 复放宿主)、std json(write_json 规范形)、smoke 腿验收。

## Global Constraints

- 轨迹值编码 = std json 写出面规范形(write_json;§5.1 字节级钉死,复用现成确定性出口)——两侧同码gen,不存在编码漂移面。
- 用例参数 = Ctron 字面量形(录制与复放共用同一字面量 → 类型语义天然一致);json 字面量透传不做(β 再议)。
- **纯函数边界(诚实登记)**:α 不做效果函数——参数带 `&Fs`/`&Cap` 或调用时钟面不在本计划;录制端不验证纯性(误录效果函数 = 录到当次环境值,复放失败即暴露,不静默)。
- 轨迹目录 `traces/` 进工件 = §3 工件布局首兑现;seal 后轨迹字节由 SHA256SUMS/self_digest 覆盖,无新防篡改机制(反投机:不另发明)。
- 载体命令:`ctron pkg trace record <src> --case <fn>=<args 字面量>(可多例)/ ctron pkg trace replay <src|工件目录>`;deep-verify 的复放腿(verify 时跑 replay)= S3-β 接线,本计划只立 `pkg trace` 独立面。
- rc 约定:record 0/2;replay 0 全符 / 1 不符(点名 fn+用例+双值)/ 2 用法。
- Ctron 代码坑位照旧(裸 `{` 转 `\{`;禁 `;`;read_file match 形);不触 emit 面(FB 泳道热区);decl 锁不动(无 compiler/src 改动,纯编排层 + smoke)。
- smoke 腿:录制→改值→复放拒 的三断言最小腿;宿主 = bin/ctron-cc(存在即跑,缺席跳过如 nc/ctron-verify 守卫先例)。

---

### Task 1: `ctron pkg trace` 命令面(./ctron sh+ps1 同文;record 侧)

**Files:**
- Modify: `./ctron`(pkg 臂扩子命令 trace;help/usage 同步)
- Modify: `ctron.ps1`(同文)

**Interfaces:**
- Produces: `ctron pkg trace record <src.ct> --case <fn>=<args 字面量> [--case ...] [--out <traces 目录>]`(缺省 = <src 所在目录>/traces/<stem>.ctrt);轨迹文件形:
  ```text
  trace "add" {
    args = "1, 2"
    expect = "3"
  }
  ```
  (args 原样存字面量串;expect = write_json 规范形;块重复可多条同名 fn。)

- [ ] **Step 1: record 实现**(shell:对每 case 生成一次性驱动文件→`bin/ctron-cc run`→stdout 收 expect)

```sh
# 核心码gen(case "$fn" 保证字面量安全:fn 名白名单 [A-Za-z_][A-Za-z0-9_]*)
gen_case_driver() {
    # $1=src.ct $2=fn $3=args 字面量 $4=out.ct
    {
        printf 'use %s.{ %s }\n\nfn main() {\n    println(write_json(%s(%s)))\n}\n' \
            "$(module_seg "$1")" "$2" "$2" "$3"
    } > "$4"
}
```
  (module_seg = src 路径→模块 use 段;单文件 α:驱动与 src 同目录放,`use` 省略——直接同目录合并声明,细节实施时定,先源码同目录合并形。)
- [ ] **Step 2: expect 采集与落盘**(json 规范形逐字;write_json 由驱动内侧引入——驱动模板 `use std.json.{ write_json}`)
- [ ] **Step 3: 自检**(手工:对 tests/artifact_demo/base/base.ct 的 area_rect(如为纯 fn)录一例,核对 expect 与手算一致)

### Task 2: replay 侧 + 拒绝诊断

**Files:**
- Modify: `./ctron` / `ctron.ps1`(trace replay 臂)

**Interfaces:**
- Consumes: Task 1 的轨迹文件形与码gen。
- Produces: `ctron pkg trace replay <src.ct|(含 traces/ 的目录)>`:重录当前值逐字比对;不符 → `trace mismatch: <fn>(<args>) 记录 <expect> 实际 <actual>`,rc=1;全符 → `trace replay OK: N 用例`。

- [ ] **Step 1: replay 实现**(读 .ctrt 块→同码gen 重跑→比对;多不符累积全点名)
- [ ] **Step 2: 自检**(正例 rc0;手改轨迹 expect 一字→rc1 点名;还原→rc0)

### Task 3: seal 编排并入 traces/ + smoke 腿

**Files:**
- Modify: `compiler/ctc.sh`(seal 编排 SHA256SUMS 的 find 行扩 `traces`:`find impl traces -type f 2>/dev/null`——traces 缺席不炸)
- Modify: `compiler/test/smoke.sh`(3t 腿:纯 fn 夹具 record→replay OK→篡改 expect→replay 拒→seal 含 traces 后 deep verify 仍过)

- [ ] **Step 1: ctc.sh 一行扩**(注意 `find impl traces` 在 traces 缺目录时 stderr 吞掉;POSIX 下 `find impl traces` 任一缺即错——用两 find 拼或先判目录存在)
- [ ] **Step 2: smoke 3t 三断言 + 全量门**
- [ ] **Step 3: 台账回写**(s0-iface 账本 S3-α 节 + spec §10 S3 行注 + memory)

## Self-Review

- spec 对位:§5.1 录制(纯函数采样)→Task 1;§5.2 复放+不符拒→Task 2(E4040 编译器码=β);§3 traces/ 布局+SHA256SUMS 覆盖→Task 3;§7.2 pkg trace 命令面→Task 1/2。FakeFs 效果函数、加载期复放腿、采样策略分层 = β/γ,已登记。
- 占位扫描:Task 1 Step 1 的 module_seg 标注「实施时定」= 有确切动作(同目录合并形),非占位。
- 类型一致:.ctrt 块形(args/expect 键)Task 1 定义、Task 2 消费、Task 3 seal 覆盖三面对齐。
