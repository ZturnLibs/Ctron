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

### P0：11 参桥 hval 差一位（>9 参调用边界）
- 现状：args11 判据 hval=10 bg=11 中 bg 已对但 hval=11（应为 10）——参 10/11 边界仍乱
- 已排除：帧串正确（FR 转储）/u 数组正确（DISPATCH 转储）/入口栈正确（memory read）
- 已试：long→int shim 回退（bg 对 hval 仍差）/int cast 改良（bg 对 hval 仍差）
- 续接 = lldb 交互单步：-g 构建 break gui_cfg → si 逐条指令观察栈写入与读取偏移
- 或者：gui_cfg C 签名改 long×11 + CTron decl I64 + 解释桥 long cast 三层同 long（待系统验证）

### P1：Todo 域包迁移回迁（设计已存档）
- 122 行域包形态本体已写好（设计含完整断言流）——回退前代码在本会话上下文
- 受阻：strlen(NULL)（frame #0 = _platform_strlen）——mirror 空 draft 的 Clay 切片
- 探针 OOB 已排除（空 todos 读 [0] 系探针自身 bug）
- **与 P0 同根疑点**：>9 参调用边界——11 参桥修好后重试迁移可能自愈

### P2：SL-8 {expr} 绑定 + on: 自动闭包
- {ident} → {expr}（纯读门 E8190）；on: → 表达式事件（自动闭包）；钩子/锚退役
- 终态用户装配 ≈8 行：gui.run(TodoApp(model: make()))
- 前置：P0 收口（骨架树 hval 正确是绑定面基座）

### P3：SL-9 热重载宿主骨架化 + 渲染回归进 CI
- 骨架原址替换（§11.5）；渲染黄金帧差分已就位（s21 安全网）

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

### 11 参桥错误签名速查
- hval=0 bg=10 = long cast 写 8 字节槽 + int shim 4 字节读（跨槽错位）
- hval=11 bg=11 = int cast 写 4 字节步长 + long shim 8 字节窗读（跨槽反向）
- 根治 = shim/声明/桥三层同类型（全 int 或全 long），当前 = 全 int（部分生效）

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

