# gui_files —— 文件查看器(GUI 域包真实 IO 首例)

`read_dir`/`read_file` 内建 + input 导航 + each 列表 + when 状态 + 文本预览:
第一个消费真实文件系统数据的 GUI 示例。目录探测 = `read_dir(子路径)` 可读性
(内建面暂无 is_dir);"打开"目录优先、退而预览文件、再而错误态上屏。

## 运行

```sh
sh run.sh          # 构建 + headless 全链路断言(列表/导航/预览/错误态,全自动)
sh run.sh --run    # 追加真实窗口(键入子目录或文件名 + 点打开/上级)
```

## 文件导览

- `app.ctml` —— 视图:标题(目录: {cwd})/ 导航行(input + 打开 + 上级)/
  when 三态(错误/预览/空目录)/ each 条目列表
- `src/main.ct` —— 模型与动作:`join`/`parent_of`/`split_entries`/`head_bytes`
  纯助手 + `try_chdir`/`show_file` IO 动作 + bind 通道分发
- `testdata/` —— 断言夹具:中英文文件名 + 中文内容 + 子目录(headless 断言
  与真窗演示共用;readdir 序不保证,断言均为顺序无关)

## 当前能力边界(如实)

- 行级点击导航 = 实例索引分发 L1 缺口(登记在案),输入导航替代
- 键入限可打印 ASCII(IME 组词随 M3);中文文件名可列表展示/打开需 ASCII 名
- 条目无类型图标/大小(内建面无 stat);readdir 序不排序
- 预览截头 240 字节单行显示(UTF-8 截尾由渲染侧容错)
