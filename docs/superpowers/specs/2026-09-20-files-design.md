# examples/files(文件查看器)——设计记录

> 状态:已实现(2026-09-20 会话;候选分析后用户选定 A 方案开工)。
> 上游:M1 能力面(s10-s13 阶梯)、gui_calc 先例。定位:**第一个消费真实 IO 数据的
> GUI 示例**——数据源从合成数据跨到文件系统。

## 0. 本文回答什么

GUI 示例至今全是合成数据(counter/calc/todo)。文件查看器用现成 builtin(read_dir/
read_file/fs_exists)把真实文件系统接进 GUI,验证"列表渲染 + 行级事件 + 真实 IO"
的组合可行性,是目录树/CSV 查看器等后续示例的地基。

## 1. 能力面用法(全部已验证,零新 shim)

- read_dir(path) → Option[Str](\n 分行;C 宿主与发射口径均为 Option 语义,2026-09-20 实证)
- 点击行(实例下标句柄 M1-b2)→ 进入目录 / 文件标记
- when(cond={isempty})空目录态;Str 绑定(path/status 标签)
- is_dir 判定 = 逐项 read_dir 探测(None 即文件)——无 is_dir builtin 的替代
- 列表排序 = 选择排序(ASCII 字节序,str_cmp 助手);".." 合成首行(path ≠ "." 时)

## 2. 结构与模型

```
files/
  Ctron.ctcl  app.ctml(view Files:path 标签 + up 按钮 + status 标签 +
              when 空态 + each 行按钮([d]/[f] 前缀 + 名字))
  src/main.ct 迷你词法(s12 快照)→ 解析(when+each+行级句柄)→ read_dir 列表装载
              → 选择排序 → 行级导航(实例下标句柄)→ headless 断言 / --run
  run.sh      构建 + headless;--run 开窗(惯例同 gui_counter)
```

模型:cur_path(Str;起始 "."),entries List[Str](原始名,".." 合成首行),
is_dir List[I32](逐项探测),status Str(行级反馈);syms[0]=行数,syms[1]=空态。

## 3. 验收(headless,命令缓冲逐字节)

1. 起始 ".":5 项(Ctron.ctcl/README.md/app.ctml/run.sh/src,ASCII 序),
   src 标 [d]、其余 [f];path 标签 "path: ."。
2. 点 src 行 → path "./src",仅 main.ct 一行 [f]。
3. 点 main.ct 行 → status "file: main.ct"(文件不可进入)。
4. 点 up → 回 ".",5 项复原。
5. 点 ".."(合成首行,src 内不可用,在 "." 无 ..)——.. 语义经 cur+"/.." 相对解析。

## 4. 边界(如实)

- 无文件内容预览(read_file 有,UI 面未做——滚动/多行文本未落地)
- 排序 ASCII 序(大写在前);无类型图标(文本前缀替代)
- 无滚动:目录条目超窗高不可见(滚动容器未实现,登记)
- Windows 路径分隔未适配(/ 拼接;发射面 CI 有 linux/darwin 臂)
