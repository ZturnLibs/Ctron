# todo —— 规范 §10 Todo 演绎(L1 域包形态门面示例)

`use std.gui` 一行:解析/布局/命中/事件循环全部在域包,用户树 = 数据 + 动作 + 视图
(src/main.ct ≈130 行 + app.ctml)。能力面串联:input 受控输入(bind)+ when 空态 +
each 列表渲染 + on:click + 标量绑定:键入文字 → add 入列 → del 末项 → 空态回归。

## 运行

    sh run.sh          # 构建 + headless 全链路断言(键入/回显/添加/列表/空态/删除)
    sh run.sh --run    # 追加真实窗口:键入文字,点 add/del,交互验收(run_kb 事件循环)

## 与规范 §10 目标形态(v10 终态)的差集(如实登记)

- per-row checkbox / 逐项删除 = 实例索引分发 L1 缺口,del 末项替代(行文本直显)
- on:submit(input 回车提交):随 M3;IME 组词(preedit):随 M3
- 中文键入:渲染管线已支持(见 gui-cjk),键入演示用 ASCII(窗口默认字体无 CJK 字形)

## 坑位(迁移实证)

- headless 视口必须容得下全部内容:Clay 对视口外元素剔除渲染命令,列表增高后
  行/空态文本从命令缓冲消失(断言面不可见)。视口 320x640。
- 零参方法发射缺口(如 `.pop()` 悬空逗号):del 以重建列表 + 成员赋值配方绕行。
- 域包形态需 std 三级解析:run.sh 发射须设 CTRON_STDPATH(旧自包含形态不需)。
