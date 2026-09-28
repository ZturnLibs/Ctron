# gui_dash —— 指标仪表盘

跨文件组件 + 内容投影的首个真实应用示范。

- `src/widgets.ct`:组件模块(StatCard 指标卡 / Panel 投影面板)——组件体仅用自家 prop,宿主内容经 `<slot/>` 注入
- `src/main.ct`:根视图 `run(Root(m: make()))` 一行装配;headless 直驱合成 fn 全链断言

运行:`sh run.sh`(headless 断言)/ `sh run.sh --run`(真窗手玩,refresh 按钮换数)
