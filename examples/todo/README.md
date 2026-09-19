# todo —— 规范 §10 Todo 演绎的当前能力版(M1-e 门面示例)

 input 受控输入 + each 行级 checkbox(done 切换)+ when 空态 + on:click 全能力面串联:
 键入文字 → add 入列 → 行级 done 切换([ ]/[x] 前缀)→ del 末项 → 空态回归。
 checkbox 为 M1-b2 行级按钮句柄实现(每行一个切换按钮 + 状态前缀)。

## 运行

    sh run.sh          # 构建 + headless 全链路断言(无显示依赖,全自动)
    sh run.sh --run    # 追加真实窗口:键入文字,点 add/del,交互验收

## 与规范 §10 目标形态的差集(如实登记)

- on:submit(input 回车提交):随 M3
- IME 组词(preedit):随 M3(当前上屏文本可达即 M0–M2 定义形态)
- 中文键入:渲染管线已支持(见 gui-cjk),键入演示用 ASCII(窗口默认字体无 CJK 字形)
- 解析器为自包含快照(P2-B 后切 use gui);严格验证归 tests/gui 阶梯(s10/s11/s12)
