# GUI 能力补齐:scroll 滚动容器 + 滚轮事件——设计记录(能力→验收→测试)

> 状态:已实现(2026-09-20 会话)。方法论(用户定):**先补能力 → 用能力做示例验收 →
> 同步补测试**。上游:规范 §13 T0(scroll 是 M0 目录组件,一直未实现)、§12.3b
> (Wheel 翻译表)、§12.2(滚动位置=运行时本地……修正:Clay v0.14 内部管偏移,
> 运行时喂增量——与规范措辞的偏差见 §4)。

## 0. 能力切片

1. **scroll 滚动容器**(解析器新容器元素 `<scroll>`;draw 行组式展开子元素;
   Clay clip.vertical + childOffset = Clay_GetScrollOffset());
2. **滚轮事件**(§12.3b Wheel:GetMouseWheelMove 合并进 poll(type 4)+
   headless 注入口 gui_inject_wheel;增量喂 Clay_UpdateScrollContainers)。

实现落点(域库单一真源 std/gui/c_src/ctron_gui.c):

- `gui_cfg2(...)`:gui_cfg 全参数 + clipv(垂直裁剪/滚动位);gui_cfg 保留原签名
  (全部既有夹具零改动);gui_cfg2 即 clipv=1;
- `gui_begin_layout`:每帧 `Clay_UpdateScrollContainers(false, (0, 累积量), 0.016)`
  后清零——滚动偏移由 Clay 内部管理(实证口径,§4 偏差登记);
- `gui_inject_wheel(v)`(headless 测试缝)/ poll 轮询 GetMouseWheelMove
  (真实窗口路径),两者同入累积器。

## 1. 验收示例:examples/files(文件查看器)

真实 IO 数据(read_dir builtin 首次进 GUI):起始 "."(示例目录,5 项确定性),
path 标签 + up 按钮 + status 标签 + 滚动列表(行按钮 [d]/[f] 前缀 + 名字,
[d]=逐项 read_dir 探测)。点目录进入、点文件 status 反馈、up 回退、
path ≠ "." 时合成 ".." 首行。列表 125px > 视口 ~118px → 滚动真实生效。

## 2. 测试(同步)

- **s18_scroll 夹具**(能力测试):20 行列表于 240 视口,滚轮注入后
  断言首行被裁剪(find_text = -1)、末行可见、回滚复原(命令缓冲逐字节)。
- **files headless**(验收测试):起始 5 项确定性断言(标记/排序/path/status)、
  进 src、文件点击反馈、up 回退、滚轮后首行裁剪。
- 回归:阶梯 19+、四既有示例。

## 3. 排序/探测

- 排序:选择排序 + str_cmp(ASCII 字节序;大写在前——如实)
- is_dir:无 builtin,逐项 read_dir 探测(None = 文件);fs_is_dir builtin
  (需 compiler-c 宿主同步)登记缓行

## 4. 与规范的偏差登记

- §12.2 "滚动位置=运行时本地":修正为 **Clay 内部管理偏移**(v0.14
  Clay_UpdateScrollContainers + GetScrollOffset 已然),运行时只喂增量——
  规范措辞回落到实现现实,经 §12.4 评审口径登记于此。

## 5. 余项

- 拖动滚动(enableDragScrolling=true 路径,需鼠标按住位移事件);
- 水平滚动;滚动条视觉;input 塌陷的 min-height(见 Ladder 记录)。
