# gui_counter —— Ctron GUI 声明式路径示例(L1 域包形态)

`.ctml` 声明视图,逻辑在 Ctron,平台全部在 `std/gui` 域包——**用户树零 `#[trusted]`、
零解析器、零事件循环**。计数器虽小,走通的是完整链路:`use std.gui` → CTML 解析 →
绑定求值 → Clay 布局 → 几何命中 → 事件分发 → 命令缓冲。

## 运行

```sh
sh run.sh          # 构建 + headless 断言 + 状态恢复往返(全自动)
sh run.sh --run    # 真实窗口:点击 +1/clear;编辑 app.ctml 保存,60 帧内原址热重载(计数保留)
```

## 文件导览

- `app.ctml` —— 视图:`view Counter` + 三个 style
- `src/main.ct`(~70 行)—— 领域:Model + act_counter(动作名 → 状态)+ 双口径装配:
  headless 走域包 `test()`(注入断言),窗口走 `run_d`(默认锚 "app.ctml" + 热重载环)
- 平台单一真源:`std/gui.ct`(域包)+ `std/gui/c_src/ctron_gui.c`(C 收缩桥)

## 形态注记

- 本示例已从"自包含快照"(913 行内嵌解析器)切包为域包形态(SL-3,2026-09-20);
  历史形态见 git 历史
- 解释口径 read_file 存在 T/Some 回归(登记收敛设计 §7),当前以原生口径为准
- L2 落地后:main 进一步收敛为 `gui.run(Counter(model: make()))`,热重载为骨架原址替换
