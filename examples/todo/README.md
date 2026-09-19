# todo —— 规范 §10 Todo 演绎的当前能力版(M1-e 门面示例)

 input 受控输入 + each 列表 + when 空态 + on:click + 标量绑定的全能力面串联:
 键入文字 → add 入列 → 列表逐行渲染 → del 末项 → 清空后空态("nothing yet")回归。

## 运行

    sh run.sh          # 构建 + headless 全链路断言(无显示依赖,全自动)
    sh run.sh --run    # 追加真实窗口:键入文字,点 add/del,交互验收

## 与规范 §10 目标形态的差集(如实登记)

- checkbox:以 del 末项按钮替代(逐项事件句柄随 M1-b2)
- on:submit(input 回车提交):随 M3
- IME 组词(preedit):随 M3(当前上屏文本可达即 M0–M2 定义形态)
- 中文键入:渲染管线已支持(见 gui-cjk),键入演示用 ASCII(窗口默认字体无 CJK 字形)
- 解析器为自包含快照(P2-B 后切 use gui);严格验证归 tests/gui 阶梯(s10/s11/s12)
