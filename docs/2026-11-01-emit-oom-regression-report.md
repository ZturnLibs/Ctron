# 回归报告:fa966b98 发射产物 loom-scale 闭包 OOM(rc=137)

- 日期:2026-11-01(loom 侧相邻仓自动化演练发现)
- 复现:loom 仓(相邻仓)`CTRON_BIN=<fa966b98 构建> sh scripts/gate-all.sh` → [6/6] native selftest(full)→ **SIGKILL rc=137 @59s**;对照 bfe5c621 构建 = 同闭包 64/0 全绿
- 范围:fa966b98 emit(含 t_remove 修复 = 自宿绿 ✓);loom main 闭包 1.66MB C → selftest 二进制 1.51MB,运行期 OOM
- 推断:容器运行时/interp 批后发射产物内存形态增重(闭包级;小套件未触)
- 影响:下游大闭包项目无法用 fa966b98+;bfe5c621 = 最后可用
- 备注:同日早前报告(2026-10-30-selfhost-build-break-report.md)之 t_remove = 本批已修 ✓;本报告 = 新回归独立项

## 勘误(2026-11-02,语言层审计)

本报告触发的两个默认值旋钮均为误归因拨动,拨正于分支 `f-emitchecks`(二分专窗收口后并 main):

1. **9cd90360 emit_checks 默认翻 off**:理由链全部来自本报告的 loom OOM——业务回归不应改语言默认。spec §4.5 明文「默认检查算术」;Rust 参考臂/native_suite 原生面本就默认 checked,翻转致 C 工具链原生面与参考实现分叉(interp panic / native 静默回绕)。且事后归因排除:d5a5be6f 双模双失败证明 GC 模式非根因,而 OOM 机制上溢出检查(纯比较+分支,零分配)不可能独立致 RSS 增长——9cd90360 与 36c8d8a8 两旋钮同误。
2. **36c8d8a8 GC 默认复归 on**:方向与 J2 语言规划一致(维持),但其「GC=1 全绿」证据未在重建 bin 上复现(双模双失败),归因注记作废。

**现状**:默认复归 checked(spec 面);除零检查脱烛台门恒检查(spec「除零永不关」,旧 off 档裸 C 除法除零 = SIGFPE)。真实根因由二分专窗在 Oct-31 批内定位(bfe5c621..fa966b98);若定谳 W2 检查算术,语言层正解 = 修缺陷,非默认关检。loom 侧按报告既有指引回退 bfe5c621 构建。

**二分参考(f-emitchecks 审计附带)**:W2 检查算术作为 OOM 候选可基本机制排除——发射臂 `ctron_w_add/sub/mul` 检查恒假(回绕后恒落 [lo,hi],运行期从未 panic;探针实证 `2147483647+1` rc=0 静默回绕),运行时形态增量 = 每次 I32 算术一次函数调用+比较,零分配零增长;fa966b98 的二进制增重为纯 text。OOM 根因应优先在容器运行时/标记面/While+Drop 栈机制候选取证。

---
## 追注(2026-11-03,loom 执行会话复验定谳)

**结论:OOM 未因 1fee8724 While 臂修复闭环;真根因 = J2 半落地(GC 默认关×分配位点已翻 gc_alloc),CTRON_GC=1 运行即全绿闭环。**

复验实录(loom 全量门 [6/6] native selftest full,1fee8724+loom 反哺两修复):
- 默认 env:rc=137(SIGKILL)三连(59s/109s/43.8s);RSS 曲线 257MB→1.93GB(5s)爆冲后死亡;死亡区 = check_cdc(pty 逐行实证末标记 »check_cdc)
- 排除复验:CTRON_CFG_EMIT_CHECKS=off 仍 137(检查算术排除,审计结论成立);CTRON_GC=0 仍 137(然 = 无效证,见下)
- **真根因**:S4③(936fe068,10-04)分配位点翻 ct_gc_alloc(cron_str_concat 等),而 driver_emit.ct:282 ct_gc_enabled = getenv 选入式(**默认关**,与 :157「默认开」注释相抵)→ check_cdc 30 万次 `b = b + str_byte(...)` 拼接全量无回收 → RSS 爆
- **闭环条件:CTRON_GC=1 → 64 检 0 败 ALL GREEN(60s)**;loom 全量门同 env ALL GREEN 首通(检查开+GC开组合)
- fa966b98 同象归因更新:彼 rc=137 = 同根(GC 默认关×gc_alloc 拼接),非 GC/检查算术本身,亦非 While 臂;While 臂修复有效但非此症
- 待办:Ctron 侧翻正 ct_gc_enabled 默认(J2 裁执合一)或拼接面 bump 回退;COVERAGE.md ③在册
