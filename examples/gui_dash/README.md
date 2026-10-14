# gui_dash —— 指标仪表盘

跨文件组件 + 内容投影的首个真实应用示范。

- `src/widgets.ct`:组件模块(StatCard 指标卡 / Panel 投影面板)——组件体仅用自家 prop,宿主内容经 `<slot/>` 注入
- `src/main.ct`:根视图 `run(Root(m: make()))` 一行装配;headless 直驱合成 fn 全链断言

运行:`ctron task ex-gui-dash`(仓库根执行:构建 + headless 全链断言)

真窗手玩(refresh 按钮换数)未收编进任务表:本任务跑绿后 `cd examples/gui_dash`
执行 `./build/gui_dash.bin` 即开真窗口;手工命令见 `ctron.ctcl` 中
`ex-gui-dash` 任务的 `desc`。
