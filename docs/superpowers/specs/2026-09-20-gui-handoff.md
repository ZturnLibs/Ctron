# GUI 泳道交接提示词

> 将本文件作为下一会话的初始上下文。所有路径/行号/复现基于 commit `bbc7a90` 树。

## 你是谁

你是 Ctron 仓库的 GUI 泳道执行者。Ctron 是一门 AI Native 系统编程语言，GUI 采用 web 式体验（CTML 标签 + CSS 子集样式 + Ctron 逻辑）。当前 L1 域包（`std/gui.ct`）已交付，双示例跑在域包上。你的任务是继续推进剩余编译器缺口修复和 L2 管线。

## 必读文件（按优先级）

1. `docs/superpowers/specs/2026-09-19-gui-use-gui-design.md` — L1/L2 收敛设计（§7 缺口登记表 ⑯-⑲ 是修复清单；§9 切片即任务队列）
2. `docs/superpowers/specs/2026-09-19-gui-master-design.md` — 总设计（样式三归属/主题包/平台适配/终形态七判据）
3. `docs/superpowers/specs/2026-09-16-gui-ctml-design.md` — 基础规范（§4 语法/§6 运行时/§11 实现管线/§13 组件）
4. `docs/superpowers/specs/2026-09-19-gui-user-surface-analysis.md` — 差距量化

## 当前状态（全部绿，2026-09-21 P0-P3 收口后）

```
自举 suite         73/73（宿主列 06e_cancel 既有分歧不变）
GUI 阶梯           24/24（含 s19_input_d）
e8_corpus          15/15（pos/neg/warn × 内嵌/独立双形态）
gui_counter        ✓ (77 行域包形态)
gui_calc           ✓ (991 行域包形态)
when+each 组合探针  ✓ 原生全绿
input 探针          ✓ 原生全绿 + 域包端到端 s19 落阶
SL-0.6 缺口登记表  ⑯⑰⑱⑲ 全闭环
SL-6 (L2-1)        ✅ 检查面对齐 + 独立 .ctml（19b41a3/bac800d）
```

## 已落库提交（按序）

```
44cf80a  fix(compile): pub struct 路由 + Box 捕获 + Void 体 + F0 映射
164f78a  feat(gui): L1 域包 std/gui.ct
b54e54f  feat(examples): 双示例切包（913→77 / 1958→991）
380b48c  fix(compile): read_file 跨宿主对齐（⑯）
4fe854c  docs(gui): 总设计（样式/平台/系统服务/终形态）
ef9ef54  docs(gui): gui_calc 设计记录
f4fa574  docs(gui): ⑰ 精确特征化
822c363  feat(gui): input 解析分支 + attrs 扩展
63e54fb  feat(gui): input 解析接线
8201056  revert(gui): std/gui.ct 回退（⑲ 触发，修复后 revert-pick）
5807a33  feat(gui): SL-5 when+each+input 路由+索引 helper
b5e419e  fix(gui): each 项文本直取
1a9ff75  feat(gui): when 条件渲染（SL-5 首片）
bc30e7a  feat(gui): 域包增量（热重载+key_name+Driver 装箱）
0a28900  fix(compile): 索引越界 panic 增加目标变量名
bbc7a90  fix(compile): SL-0.6 批次六修（⑰ If 路由 + clb If/Match 穿透在此）
430218c  fix(compile): SL-0.6-⑰ 收口——F0 零参 fn/闭包端到端放行
681fd7b  fix(compile): SL-0.6-⑱ 解释口径 struct-List 字段索引写透
06d112a  test(gui): SL-5 input 域包端到端落阶 s19_input_d（阶梯 24/24）
17e0b22  docs(gui): 交接文档 P0-P3 收口更新
19b41a3  feat(gui): SL-6a+6b+6d——检查面认领 + E8xxx + 语料 11 件
bac800d  feat(gui): SL-6c——独立 .ctml 检查入口（双形态语料 15/15）
```

## 关键技术事实（省去重新发现的时间）

### 语言能力现状（SL-0 探针实证）

| 能力 | 状态 | 说明 |
|---|---|---|
| F1 闭包（1 参 + Box 捕获） | ✅ 双宿主 | gui 域包全部钩子的基础 |
| 闭包体单调用 | ✅ 原生 | Void 体已修（typeof 判 v） |
| 闭包体 If 语句 | ✅ 双宿主 | bbc7a90 If 路由 + clb 穿透；p5 探针绿 |
| 空闭包体 `|| {}` | ✅ 双宿主 | 430218c（发射 np 闸门 + F0）；解释口径同绿 |
| F0 零参 fn 类型 | ✅ 双宿主 | 430218c 三闸门 + ct_fn0/ct_cfn0 + far==0 调用点 |
| struct 字面量位 ctor | ✅ 双宿主 | ⑲ 已闭环（f0lit/f0lit2 嵌套乱序变体绿），配方可退役 |
| pub struct/extern 导入 | ✅ 已修 | parse_decl 尾槽 pub + parse_pkg 判定 |
| List 推断局部直接索引 | ✅ 双宿主 | li.ct 探针绿；gt_index/gt_last 对该形态不再必需 |
| Member 读 List 字段引用 | ✅ 双宿主 | mi2.ct 实证 |
| Box[Struct] 字段写 | ✅ 双宿主 | L1 Model 状态变异的基础 |
| struct List 字段索引写 | ✅ 双宿主 | ⑱ 681fd7b（解释口径 Member 基座 Index 赋值） |

### 域包架构（std/gui.ct 单文件形态）

```
std/gui.ct (~1100 行)
  ├── extern 桥（ctron_gui.c 29 函数收编）
  ├── 迷你词法（gws/gword/glit/gstr/gspan）
  ├── CTML 解析（gt_parse → GuiTree：fc/ns 树 + npre/nbid/npost + 事件表）
  ├── 样式查询（gt_cls_prop 多类后者覆盖）
  ├── 运行时（rt_emit 递归布局/rt_bind_text 绑定/rt_hit_name 命中/rt_window_loop 循环）
  ├── 热重载（rt_window_loop_d 60 帧比对 app.ctml 原址重解析）
  ├── Driver（d_click/d_type_char/d_expect_text/d_expect_absent/d_frame）
  └── 入口（run/run_kb/run_d/run_kb_d/test）
```

用户代码形态：
```ct
use std.gui.{run_kb_d}
fn main() -> I32 {
    var src: Str = read_file("app.ctml")   // 发射锚
    var m = Box[Model](Model { ... })       // Box[Struct] 可变引用
    run_kb_d(src, "标题", 320, 420,
        |buf| buf.push(m.disp),             // 绑定：buf[0]=名 → push 值
        |name| dispatch(name, m),           // 点击 → 动作名
        |k| dispatch(key_name(k), m))       // 键盘 → 语义名
    return 0
}
```

### 通道配方（SL-0.6 后大部分已退役；存量域包代码仍按配方写，新代码可用直形态）

| 约束 | 配方 | 来源 |
|---|---|---|
| 闭包参数限指针宽度 | Box[T] 装箱 struct | SL-0 探针 |
| 闭包体保持单调用 | 控制流提具名 fn | SL-0 探针 |
| 避免空闭包体 `{}` | 至少一条语句 | SL-0 探针 |
| List 索引读走 helper | gt_index/gt_last | SL-2v 实证 |
| bind 通道 List 缓冲 | buf[0]=名 → push 值 | SL-2v 配方 |

### 修复配方（已验证的编译器修复）

| 修复 | 文件 | 变更 |
|---|---|---|
| pub struct/extern 路由 | parse_decl.ct | pub 分支补 struct/enum/extern + 尾槽 "pub" |
| loader 尾槽可见性 | parse_pkg.ct | md[len-1] == "pub" → vis=true |
| Box 捕获截断 | trans_conc.ct | ct_cap_decl B: 分支经 long 中转 |
| Void 体闭包 | trans_conc.ct | typeof 判 v → 语句形式 |
| F0/F4+ ctype | trans_ty.ct | F/G 首字符泛化 |
| read_file 对齐 | eval_call.ct | 直返 vS(s) 不走 T/Some |
| Index 越界诊断 | eval_run.ct | panic 增 base/contlen |

## 剩余任务（按优先级，2026-09-21 更新）

### ~~P0：⑰ 闭包体 If/Match/空体发射修复~~ ✅ 已收口

- bbc7a90 落了 trans_stmt.ct If 路由 + sem_spawn.clb If/Match 穿透（交接旧文称"If 路由未提交在工作树"系笔误，实已随批落库）
- 430218c 放行 F0 链：ct_ty_code/ct_typeof/Clov 三处 arity<1 闸门 + ct_fn0/ct_cfn0 typedef + far==0 调用点实参表置空 + extern 边界 F0
- 验证：p5_closures.ct 原生 rc=0（If 体 ✓ 空体 ✓ 零参 ✓）；解释口径空体/零参同绿

### ~~P1：⑱ 解释口径 struct-List 值模型~~ ✅ 已收口（681fd7b）

- 交接登记的别名形态（var fcl = t.nfc; fcl[id]=kid）探针实测已绿；真红是**直接形态** `t.nfc[id] = kid`（eval_run Assign 只认 Ident 基座，panic "assign target:Index"）
- 修复：eval_run.ct 补 Index 基座=Member 分支（Member 读 BOX 自解引用 + u_field 共享节点原地写透）
- 验证：mi4 三形态（直接/Box 中转/Str 别名）双口径全绿

### ~~P2：SL-5 input 端到端验收~~ ✅ 已落阶（06d112a）

- 落为 tests/gui/s19_input_d 常设夹具（域包形态，阶梯 23→24）
- 验证：d_type_char 注入 h/i → d_expect_text "hi"；backspace 259 → "h"

### ~~P3：SL-0.6 余项~~ ✅ 全闭环（无需再动）

- ⑲ 字面量位 ctor：`titles: List[Str]()` 嵌套/乱序变体双口径实测已绿（f0lit/f0lit2 探针），配方可退役
- read_file 解释口径 T/Some：RF-OK 无回归
- 零参闭包 arity 门槛：430218c 已修
- 附带实证：List 推断局部直接索引（var w = v; w[1]）双口径已绿，gt_index/gt_last helper 对该形态不再必需

### P4：L2 SL-6..9（SL-6 ✅ 已收口 19b41a3+bac800d；SL-7 起待动工）

- ~~SL-6 (L2-1)：gui_parse/gui_check 对齐 + M1-d 合流 + 独立 .ctml 检查~~ ✅ 完成：
  gui_ck_elem 递归走查认领 when/each/input；E8100/E8110/E8120/E8193 注册；e8_corpus
  15/15（pos/neg/warn × 内嵌/独立双形态）；坑位与 SL-7 入口设计见
  plans/2026-09-21-sl6-gui-compiler-alignment.md
- SL-7 (L2-2)：骨架 IR 三产物——**前置：GuiNode C 布局落 shim 属域库单一真源变更
  （§4.5），与发射泳道共辆，动工前登记 roadmap**；入口设计已写在计划文档
- SL-8 (L2-3)：{expr} 绑定 + on: 自动闭包；钩子/锚退役（前置 SL-7）
- SL-9 (L2-4)：热重载宿主骨架化 + 渲染回归进 CI（前置 SL-8）
- 终锚：Todo v10 照抄能跑

## 验证命令

```bash
# 自举 suite
python3 compiler/test/suite.py 2>&1 | grep "合计"

# GUI 阶梯
sh tests/gui/run.sh 2>&1 | tail -1

# gui_counter 切包验收
cd examples/gui_counter && sh run.sh

# gui_calc 切包验收
cd examples/gui_calc && sh run.sh

# when+each 探针
cd /tmp/sl5b && export CTRON_STDPATH=/Users/zyj/Zturn/Ctron/std && /Users/zyj/Zturn/Ctron/compiler/bin/ctron-emit run src/main.ct > w.c && cc -O1 -w -I/Users/zyj/Zturn/Ctron/vendor/gui/clay -I/Users/zyj/Zturn/Ctron/vendor/gui/raylib -o w w.c /Users/zyj/Zturn/Ctron/std/gui/c_src/ctron_gui.c /Users/zyj/Zturn/Ctron/vendor/gui/build/libraylib.a -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo && ./w
```

## 注意事项

1. **并行泳道**：compiler/src 有并行泳道在飞（net/server/launch），改动前 `git status compiler/src/` 确认无冲突
2. **工具链重建**：改 compiler/src 后须 `cd compiler && sh build.sh && sh native.sh`（~2 min）
3. **CTRON_STDPATH**：跨目录运行需 `export CTRON_STDPATH=$ROOT/std`
4. **发射锚**：main 首个 `read_file("app.ctml")` 是发射锚（CWD 相对；L2 内嵌形态后消失）
5. **不碰的文件**：`compiler-c/`（seed）、`compiler-rust/`（R 线）、`selfhosted/`（自举链）——归对应泳道
