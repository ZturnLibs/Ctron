# GUI 泳道交接提示词

> 将本文件作为下一会话的初始上下文。所有路径/行号/复现基于 commit `bbc7a90` 树。

## 你是谁

你是 Ctron 仓库的 GUI 泳道执行者。Ctron 是一门 AI Native 系统编程语言，GUI 采用 web 式体验（CTML 标签 + CSS 子集样式 + Ctron 逻辑）。当前 L1 域包（`std/gui.ct`）已交付，双示例跑在域包上。你的任务是继续推进剩余编译器缺口修复和 L2 管线。

## 必读文件（按优先级）

1. `docs/superpowers/specs/2026-09-19-gui-use-gui-design.md` — L1/L2 收敛设计（§7 缺口登记表 ⑯-⑲ 是修复清单；§9 切片即任务队列）
2. `docs/superpowers/specs/2026-09-19-gui-master-design.md` — 总设计（样式三归属/主题包/平台适配/终形态七判据）
3. `docs/superpowers/specs/2026-09-16-gui-ctml-design.md` — 基础规范（§4 语法/§6 运行时/§11 实现管线/§13 组件）
4. `docs/superpowers/specs/2026-09-19-gui-user-surface-analysis.md` — 差距量化

## 当前状态（全部绿）

```
自举 suite         73/73
GUI 阶梯           23/23
gui_counter        ✓ (913→77 行)
gui_calc           ✓ (1958→991 行)
when+each 组合探针  ✓ 原生全绿
input 探针          ✓ 原生全绿
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
bbc7a90  fix(compile): SL-0.6 批次六修
```

## 关键技术事实（省去重新发现的时间）

### 语言能力现状（SL-0 探针实证）

| 能力 | 状态 | 说明 |
|---|---|---|
| F1 闭包（1 参 + Box 捕获） | ✅ 双宿主 | gui 域包全部钩子的基础 |
| 闭包体单调用 | ✅ 原生 | Void 体已修（typeof 判 v） |
| 闭包体 If 语句 | ✗ 原生 | shim 体受限发射器写 `ct_stmt:If` 到 C |
| 空闭包体 `|| {}` | ✗ 解释段错误 | 解析器/发射器缺口 |
| F0 零参 fn 类型 | ✗ typeof 归 "i" | trans_ty.ct:813 cvn<1 门槛 |
| struct 字面量位 ctor | ✗ 发射 Index(List,Str) | `titles: List[Str]()` 触发 |
| pub struct/extern 导入 | ✅ 已修 | parse_decl 尾槽 pub + parse_pkg 判定 |
| List 推断局部直接索引 | ✗ env 丢参型 | 配方 = gt_index/gt_last helper |
| Member 读 List 字段引用 | ✅ 双宿主 | mi2.ct 实证 |
| Box[Struct] 字段写 | ✅ 双宿主 | L1 Model 状态变异的基础 |

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

### 通道配方（SL-0.6 前的约束，配方绕行）

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

## 剩余任务（按优先级）

### P0：⑰ 闭包体 If/Match/空体发射修复

- 复现：`/tmp/sl0/p5_closures.ct`（If 体闭包 + 空体闭包）
- 定位：trans_stmt.ct:1400 fallback（shim 体 If 语句无路由）+ sem_spawn.ct clb（If/Match 捕获不穿透）
- 修复：ct_stmt 补 If 路由 → ct_if_stmt（1335 已有 If-as-Expr 路径可参考）；clb 补 If/Match 穿透
- 验证：p5_closures.ct 原生 rc=0（If 体 ✓ 空体 ✓）
- **已有部分修复**：trans_stmt.ct If 路由已加（未提交，工作树有）；clb If/Match 已加（已提交）

### P1：⑱ 解释口径 struct-List 值模型

- 复现：`var fcl: List[I32] = t.nfc; fcl[id] = kid` → 写入副本丢弃
- 定位：eval Member 读 struct List 字段返回副本语义
- 影响：域包多文件拆分 + each 嵌套在解释口径的正确性
- 修复：eval Member 分支对 List 字段返回引用

### P2：SL-5 input 端到端验收

- 代码 100% 就位（`/tmp/sl5/src/main.ct`）
- 前置：⑰（闭包 If 体配方已有，可先绕行）
- 验证：d_type_char 注入 + d_expect_text 断言

### P3：SL-0.6 余项

- ⑲ 字面量位 ctor（提升配方绕行）
- read_file 解释口径 T/Some（已修 ✓ 确认无回归）

### P4：L2 SL-6..9

- gui_parse/gui_lower 三产物管线（骨架 IR + 槽表 + E8xxx）
- 前置：发射泳道评审 + L1 域包稳定
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
