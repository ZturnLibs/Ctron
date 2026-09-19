# gui_counter —— Ctron GUI 声明式路径示例

`.ctml` 声明结构+样式,Ctron 写逻辑:`.ctml` 解析 → `{ident}` 绑定 → `on:click` 事件 →
Clay 布局 → 命令缓冲 flush 绘制。两个按钮(+1 / clear)演示多元素、多事件与
**几何命中**(点击命中矩形来自布局回填,不写死坐标)。

## 运行

```sh
sh run.sh          # 构建 + headless 断言 + 快照恢复往返证明(无显示依赖,全自动)
sh run.sh --run    # 追加启动真实窗口,真实点击交互
sh run.sh --hot    # 热重载环:编辑 app.ctml 保存 → 自动重编译+重启,计数经快照恢复
```

## 热重载

**解释口径(W4-E3,`--run` 即得)**:窗口循环每 60 帧比对 `app.ctml` 内容,变更即
**进程内原址重解析**——不重启、计数(model)保留。默认 headless 验收含 E3 探针
(重载 `app2.ctml`:gap 变更生效 + count=2 保留的全自动断言)。

**原生口径(W4,裁决 1A,`--hot`)**:"保存即重编译+重启,model 经快照恢复":
应用启动时自 `CTRON_GUI_STATE=count=N` 环境恢复 model,退出前经 stdout 回传快照;
`--hot` 环监听变更 → 杀进程 → 重编译 → 注入快照重启。状态走环境握手而非 argv——
`run <file>` 是编译器自举锚约定(会顶替 read_file 锚)。

两条口径均已在当前能力面闭环。编辑中途保存导致的解析失败会 panic 退出
(错误容忍随 W3 检查面进运行时,登记)。

## 文件导览

- `app.ctml` —— UI 结构与样式:`view Counter` + 三个 `style`(含 `btn-danger` 变体)
- `src/main.ct` —— 入口:`CTRON_GUI_HEADLESS` 环境变量切换 headless 断言 / 窗口循环;
  含迷你 CTML 解析器(同源自 tests/gui 泳道)、绑定求值、事件分发、几何命中
- C shim **不在示例树内**:run.sh 链接域库单一真源 `std/gui/c_src/ctron_gui.c`
  (Clay 布局桥 + 命令探针 + raylib 绘制口;W6 换面后应用源码树零 C)

## 当前能力边界(如实)

- CTML 面为 MVP 子集:限深 1(根容器 + label/button 叶子)、绑定 `{ident}` 单符号、
  事件仅 `on:click`;`each`/`when`/`input` 等随 M1 全量语义落地
- 窗口文本用 ASCII:Clay TEXT 命令走 raylib 默认字体,无 CJK 字形——中文渲染见
  `examples/gui_cjk`;Clay→FreeType 渲染集成随 M3
- 解析器为自包含快照:包级 `use gui` 随 P2-B 落地后切换为正式形态
- C 链接域库单一真源 `std/gui/c_src/`(W6 换面已落):示例树零 C;bind 层把 extern
  声明收编为 `std/gui` 模块随 P2-B,用户写 C 仅剩第三方接入逃生口场景
- 严格验证归 tests/gui 阶梯(s6 绑定竖切 / s7 窗口);本示例是门面,不是回归夹具
