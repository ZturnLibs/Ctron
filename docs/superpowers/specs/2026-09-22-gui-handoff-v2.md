# GUI 泳道交接提示词 v2（2026-09-22 深夜）

> 将本文件作为下一会话初始上下文。续接基线 = 当前 HEAD。

## 你是谁
Ctron 仓库 GUI 泳道执行者。当前阶段：SL-7 双口径骨架管线主体闭环（β1 树等价/β3 发射口径静态构造/β2 解释口径节点包裹），SL-6 检查面全套（E8xxx+独立 .ctml）已收口。

## 必读文件（按优先级）
1. docs/superpowers/plans/2026-09-21-sl6-gui-compiler-alignment.md —— SL-6/7β 全量实施记录+根因+续接动作
2. docs/superpowers/specs/2026-09-20-gui-handoff.md —— SL-6 前交接（背景）
3. docs/superpowers/specs/2026-09-19-gui-use-gui-design.md —— §5 L2 管线/§9 切片
4. docs/superpowers/specs/2026-09-16-gui-ctml-design.md —— §4.4 诊断码/§11.1 管线

## 当前绿态
- 阶梯 27/27（含 s19_input_d/s20_embed/s21_frame_golden/s22_sk_equiv/s23_sk_native）
- suite 73/73（宿主列 72/73 既有分歧不变）
- e8_corpus 15/15（pos/neg/warn × 内嵌/独立双形态）
- args11 hval=10 bg=11 判据全对（CTRON_GUI_TRACE=1 下 cfg 行）

## 已落库关键提交（gui 泳道）
- 263cbf6 SL-7β3 发射口径 C 静态构造（零运行时 CTML 解析）
- a862e5b SL-7β1 编译期骨架解析树等价（s22 双端差分）
- da8cb00 SL-7β2 解释口径 gui_sk_load 节点包裹装配
- 87e7fde SL-7α 内嵌 view/style 块端到端（s20_embed 独立运行）
- a8b7974 11 参 extern 桥根修三层 long 对齐
- 2208e0f s21 渲染黄金帧差分落阶
- 19b41a3/bac800d SL-6 检查面+独立 .ctml（语料 15/15）

## 剩余任务（按优先级）

### P0：11 参桥 hval 差一位 —— ✅ 已收口（d73afcc，2026-09-22 深夜）
- 真相：hval=11 系 a8b7974 半 int 态（桥 int cast + shim long）在重建二进制上的读数；
  工作树遗留的全 int 补完（shim int + 声明 I32）实测判据即全对
- 根因层级修正：本机为 ARM64（AAPCS64）——栈参 int 按 4 字节打包、long 按 8 字节
  步长（x86-64 SysV"每栈参恒 8 字节槽"推理不适用本机）；两个错位方向自此一格
- 收口 = shim/声明/桥三层全 int；≥9 参 extern 的事实契约 = int 编组（声明须 I32）
- 验证：args11 hval=10 bg=00000b 全对；阶梯 23/27（4 红均已登记跨泳道）；
  e8 15/15；suite 73/73（宿主列 72/73 既有分歧）；详见实施记录"P0 收口"节

### P1：Todo 域包迁移回迁 —— ✅ 已收口（bdb4265/94fec7b，2026-09-22 深夜）
- 迁移落地：examples/todo 1242 → 130 行域包形态，headless 断言流
  （键入/回显/添加/列表/空态/删除）+ --run run_kb 双口径全绿
- 根因反转：上回二试"when 不渲染"疑云非 11 参桥，系 **Clay 视口剔除**
  （320x240 视口下内容超高后渲染命令被剔除）——修复 = 视口 320x640
- 新增阶梯夹具 s24_model_d（Box 活模型+when+each+input+多帧全组合，阶梯 27→28）
- 坑位入档：Clay 视口剔除判定法（trace/命令缓冲对读）、零参方法发射缺口
  （.pop() 绕行配方）、域包形态 run.sh 须设 CTRON_STDPATH

### P2：SL-8 {expr} 绑定 + on: 自动闭包 —— ◐ 8a/8b 已落库（02e65b8/88f3b7a，2026-09-23）
- 8a 落地：花括号槽表达式化（规范形 = 去空白原文，四路径统一捕获）+ E8190
  纯读门（check 面）+ e8 语料 19 + s22 表达式槽双端差分锁定
- 8b 落地：域包受限纯读求值器（五级文法 + i:/b: 带型通道）+ cond/叶/bind
  三槽接线 + s25_expr_d 入阶梯（阶梯 29）
- 8c（on: 自动闭包 + gui.run 单入口 + props + 钩子退役）**已登记阻塞**：
  需动 trans/emit 面，trans_emit.ct 为 peer 在制品——待其落库后开片；
  切片计划与已就位基座清单见 docs/superpowers/plans/2026-09-23-sl8-expr-binding.md

### P3：SL-9 热重载宿主骨架化 + 渲染回归进 CI —— ✅ M0-M2 面收口（eff6f71，2026-09-23）
- 渲染回归进 CI：**核实已覆盖**——ci.sh [8/9] 挂 GUI 阶梯，阶梯含
  s21_frame_golden 黄金帧差分
- 热重载域包验收：**s26_reload_d 入阶梯**（阶梯 30）——run_kb_d 60 帧内容
  比对 → gt_parse 原址重解析；探针模式断言改写 app.ctml（样式+结构）后
  新结构入帧且闭包 Box 状态保留（n=7）；w4 app.txt 残留顺手根治
- 余项登记：焦点/IME 保留 = §6.3 M3 硬验收（依赖 IME 组词面，随 M3）；
  原生口径快照恢复 = 裁决 1A（gui_counter CTRON_GUI_STATE 协议已示范）

### 终锚：Todo v10 照抄能跑

## 关键技术事实（续接必读）

### 编译器源码字符串规则（三犯即炸 emit 链）
- 开括号 `{` 在字符串/注释中必须 `\{` 转义，否则 E1001 未终止的插值
- 花括号比较一律 byte_at 字节码（含前瞻比较 tks[k+1]== 也不能写字面 {）
- 闭括号 } 在字符串中可裸写（合法）

### 域包词法 4 处适配点（gui_blocks_src 重建器）
- </ 粘连（容器闭合裸字节探测不容 '< /'）
- ={ 粘连（attr 值花括号探测裸字节）
- 行尾 ~ 剥除（域包词法不认 W1 dump 标记）
- 空格化 token 全容忍（glit 前置 gws）

### 11 参桥错误签名速查（ARM64 修订版）
- 平台事实：AAPCS64 栈参 **int = 4 字节打包**，**long = 8 字节步长**；≤8 参走寄存器（x0-x7）不受累
- hval=0 bg=10 = long 调用 8 字节步长写 + int shim 4 字节打包读（半 int 前身）
- hval=11 bg=11 = int cast 调用 4 字节打包写 + long shim 8 字节窗读（a8b7974 半 int 态）
- 根治 = shim/声明/桥三层同类型——已收口为**全 int**（d73afcc）；≥9 参 extern 声明一律 I32

### 调试方法论
- println 缓冲在 panic/SEGV 时丢失——trace 用 panic 标记或 stderr fprintf
- lldb -g 构建：ccr_dbg.c cc -g -O0 → break fn → frame variable
- 工作树对照实验必须双态完整重建（peer 并发构建使二进制翻转）
- /tmp 复现件会被系统清理——重要复现件一律内嵌文档或入库
- peer 并发 build 竞态使 build 行数波动——回归前静默重跑至计数连三次一致

## 已登记跨泳道问题（非 gui 泳道）
- 4 旧形态夹具红（s3/s4/s5/s18）= peer parse_pkg 菱形依赖修复+use 跨行交互
- 域包 test() 驱动解释口径整体红（⑳ 家族 struct-List 读残留）
- 字面量叶文本含标点时重建插空格（骨架 IR 槽表直通根治）

## 验证命令速查
```bash
# 自举 suite
python3 compiler/test/suite.py 2>&1 | grep 合计
# GUI 阶梯
sh tests/gui/run.sh 2>&1 | tail -1
# E 语料
sh tests/gui/e8_corpus/run.sh 2>&1 | tail -1
# args11 判据（11 参桥）
CTRON_GUI_TRACE=1 CTRON_STDPATH=$PWD/std compiler/bin/ctron-cc run /tmp/skcmp/args11.ct 2>&1 | grep 'cfg dir'
# 骨架树等价
sh tests/gui/s22_sk_equiv/run.sh
# 渲染黄金帧
sh tests/gui/s21_frame_golden/run.sh
```

## 注意事项
1. **并行泳道**：peer 持续在 lex/parse/trans/eval 落库——改动前 git status 重新对齐
2. **提交纪律**：pathspec 限定；commit 后 --stat 核对文件清单防卷入 peer 暂存
3. **工具链重建**：改 compiler/src 后 sh build.sh && sh native.sh（~2 min）
4. **CTRON_STDPATH**：跨目录运行需 export CTRON_STDPATH=$PWD/std
5. **不碰**：compiler-c/（seed）、compiler-rust/（R 线）、peer 在制品文件

