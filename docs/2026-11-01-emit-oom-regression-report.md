# 回归报告:fa966b98 发射产物 loom-scale 闭包 OOM(rc=137)

- 日期:2026-11-01(loom 侧相邻仓自动化演练发现)
- 复现:loom 仓(相邻仓)`CTRON_BIN=<fa966b98 构建> sh scripts/gate-all.sh` → [6/6] native selftest(full)→ **SIGKILL rc=137 @59s**;对照 bfe5c621 构建 = 同闭包 64/0 全绿
- 范围:fa966b98 emit(含 t_remove 修复 = 自宿绿 ✓);loom main 闭包 1.66MB C → selftest 二进制 1.51MB,运行期 OOM
- 推断:容器运行时/interp 批后发射产物内存形态增重(闭包级;小套件未触)
- 影响:下游大闭包项目无法用 fa966b98+;bfe5c621 = 最后可用
- 备注:同日早前报告(2026-10-30-selfhost-build-break-report.md)之 t_remove = 本批已修 ✓;本报告 = 新回归独立项
