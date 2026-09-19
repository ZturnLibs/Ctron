# M1 绑定与事件面:each/when/input——实施计划

> 上游:gui 规范 §6(运行时与数据流)/§7(M1)/§13(组件目录);MVP 阶梯计划第二波。
> 前置:W1–W6 已闭环(2026-09-19);现能力面 = vbox/hbox/label/button + `{ident}` 单绑定 +
> `on:click` + 运行时解析 + 双口径热重载 + 零 C 用户树。
> M1 完成判据:规范 §10 Todo 演绎的当前能力版可写可测(列表 + 条件 + 文本输入)。

## 切片

### M1-a `when` 条件渲染(最薄,先行)
- 语法:`<when cond={flag}> leaf* </when>`(限深:when 只包叶子;cond = 符号名,I32 0/1 真值)
- 解析:parse_view 子元素循环分派 when/leaf;when 的 cond 名入 cond_nm 表,
  子叶子平铺进既有平行表(tags/cls/pre/bid/post/ev_*),el 编号连续
- 求值:cond_ix(绑定索引)解析同 bid_sym 时机;draw_frame 对隐藏叶子跳过
  (不产命令);事件/hit-test 天然正确——隐藏按钮无 RECT 命令,几何命中不命中;
  后续元素几何自动回流(命令缓冲回填的红利)
- 验收:新阶梯夹具 s10_when——toggle 按钮翻转 show 绑定,断言命令数增减、
  隐藏后旧坐标点击不命中(几何回流负例)、恢复后再现、when 内按钮事件可达
- 交付物:tests/gui/s10_when/(内嵌解析器快照 + 扩展),收编阶梯

### M1-b `each` 列表渲染
- 语法:`<each item in={items}> leaf* </each>`(items = List[Str] 绑定;item 文本插值)
- 模型扩展:syms 表从标量表扩展为可含 List 值(绑定值类型化——E8110 的地基);
  draw_frame 对 each 展开为 N 份叶子
- key 语义:首版按索引对齐(规范 §6.1 允许,前提=each 体无输入控件;E8193 随 M1-d)
- 验收:s11_each——按钮 push/remove 列表项,断言命令序列与文本逐字节

### M1-c `input` 受控输入
- 语义:bind Str + 焦点(运行时本地态,§4.3 契约 5)+ 键盘事件(§12.3b 翻译表
  KeyDown→字符;M0–M2 口径:上屏文本可达、IME 组词不可见即定义形态)
- 依赖 shim:KeyDown 事件携带键码/字符(现 shim 事件表有 key 字段,S4 已建)
- 验收:s12_input——注入键序列,断言 model Str 与显示文本一致;焦点点击切换
- 注:composer/preedit 随 M3(淘汰线项,单独排)

### M1-d 编译器侧对齐 + W3 余码
- gui_parse/gui_check 认领 when/each/input(E8100 白名单扩展);
  E8180(each key)、E8193(each 无 key + 输入控件)语料;
  `;` 分隔等两形态语法分歧一并对齐(规范 §11.1 同语法承诺)
- std/gui/parse.ct(canonical)同步 M1-a–c 扩展(P2-B 前无消费者,登记同步义务)

### M1-e Todo 演绎
- 规范 §10 的当前能力版:input(draft)+ each(todos)+ when(空态)+ checkbox 位
  checkbox 未落地前以 toggle 按钮替代,登记差集
- 验收:examples/todo 或阶梯夹具,headless 全脚本断言

## 风险

- 绑定值类型化(M1-b)触碰 syms 表结构——s5–s7 夹具冻结快照不受影响,
  新夹具走新表;canonical 同步时一次性迁移
- input 的焦点/键盘是运行时本地态首次落地(§4.3 契约 5)——tab 焦点环随 M3
- 编译器侧对齐(M1-d)动 gui_parse/gui_check——动工前查 peer 占用
