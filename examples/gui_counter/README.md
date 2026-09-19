# gui_counter —— Ctron GUI 声明式路径示例

`.ctml` 声明结构+样式,Ctron 写逻辑:`.ctml` 解析 → `{ident}` 绑定 → `on:click` 事件 →
Clay 布局 → 命令缓冲 flush 绘制。两个按钮(+1 / clear)演示多元素、多事件与
**几何命中**(点击命中矩形来自布局回填,不写死坐标)。

## 运行

```sh
sh run.sh          # 构建 + headless 断言(无显示依赖,全自动)
sh run.sh --run    # 追加启动真实窗口,真实点击交互
```

## 文件导览

- `app.ctml` —— UI 结构与样式:`view Counter` + 三个 `style`(含 `btn-danger` 变体)
- `src/main.ct` —— 入口:`CTRON_GUI_HEADLESS` 环境变量切换 headless 断言 / 窗口循环;
  含迷你 CTML 解析器(同源自 tests/gui 泳道)、绑定求值、事件分发、几何命中
- `c_src/ctron_gui.c` —— C shim:Clay 布局桥 + 命令探针 + raylib 绘制口
  (同步自 tests/gui/s7_window,增量 `gui_cmd_h100`;阶梯修复时随 PR 手动同步)

## 当前能力边界(如实)

- CTML 面为 MVP 子集:限深 1(根容器 + label/button 叶子)、绑定 `{ident}` 单符号、
  事件仅 `on:click`;`each`/`when`/`input` 等随 M1 全量语义落地
- 窗口文本用 ASCII:Clay TEXT 命令走 raylib 默认字体,无 CJK 字形——中文渲染见
  `examples/gui_cjk`;Clay→FreeType 渲染集成随 M3
- 解析器为自包含快照:包级 `use gui` 随 P2-B 落地后切换为正式形态
- `c_src/` shim 为**过渡形态**(GUI 规范 §11.2 增补):bind 层(MVP 阶梯 W6)落地后
  删除——目标应用源码树零 C;用户写 C 仅剩第三方接入逃生口场景
- 严格验证归 tests/gui 阶梯(s6 绑定竖切 / s7 窗口);本示例是门面,不是回归夹具
