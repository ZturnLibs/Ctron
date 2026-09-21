# SL-6 (L2-1) 实施计划：编译器 GUI 前端对齐 + M1-d 合流

> 2026-09-21。设计依据：specs/2026-09-19-gui-use-gui-design.md §5.1/§5.3/§9 SL-6 行；
> 诊断码规范 = specs/2026-09-16-gui-ctml-design.md §4.4；语法事实源 = std/gui.ct（域包解析器）。
> 基线：430218c..17e0b22（suite 73/73、阶梯 24/24、e8_corpus 5 件）。

## 语法事实（照抄域包解析器，不凭规范想象）

- 标签集（rt_emit 认领面）：`vbox hbox label button when each input`
- `<when cond={名}>…</when>`：包装元素，cond 必须花括号绑定名（SL-5 通道）
- `<each item in={列表名}>…</each>`：**itemvar 是位置头**（先于 attrs 解析，非属性）
- `<input bind={名} placeholder="…"/>`：**自闭合**（`/` 由调用方消费）
- 属性值两形态：`{单词}`（绑定名）与 `"字符串"`；`on:名={函数名}`
- 叶文本 `{ident}`：插值绑定标记

## 切片

### SL-6a+6b：检查面/LOWER 认领 + 语法可达诊断（一氏重写走查）

1. gui_ck_tag 白名单对齐域包（+when/each/input）
2. gui_ck_leaf → **gui_ck_elem 递归走查**（限深 8 守卫）：包装元素子树、自闭合、
   花括号属性值、each 位置头（裸词仅 each 头合法，余者 E8100）
3. 语法可达诊断（本片注册）：
   - E8100 props 缺失：input 无 bind / when 无 cond / each 无 in
   - E8110 属性契约：cond/bind 值非 `{名}` 形态（给字符串 = 绑定契约不符）
   - E8120 事件处理器未声明：on: 的 fn 名不在本文件 Fn/FnExt 声明表（签名级=后续 sem 联动）
   - E8193 警告级：each 体含 input 型控件（key 原语 L1 未有，索引对齐即串位风险）
     ——**警告入独立 gwarns 通道，不置 rc**（规范“警告级可关”）
4. gui_lower_element dump 同步认领（dump 与 check 同构走查）
5. driver_check：gwarns 打印不置错（--dump-gui 旗下，不动默认 check 路径）

### SL-6c：独立 .ctml 检查入口

- check 驱动对 `.ctml` 后缀分流：标记 tokenizer（<>=:{}"/ 字符类 + 词 + 串）→
  平衡扫描包成 GuiBlock File 节（复用 [1]kind [2]name [3]ntoks [4]joined 契约）→
  gui_check_file/gui_dump_file 同径。内嵌/独立同语法同路径（§5.1 语法缝）。

### SL-6d：E 语料黄金基线扩容 + runner 警告类

- 新语料：when/each/input 合成 pos（回归锁）；E8100×3（props 缺失族）；
  E8110（cond 字符串）；E8120（handler 未声明）；E8193 warn 级
- e8_corpus/run.sh 增 `*.warn.ct` 类：期待 rc=0 且输出含期望码
- 顺带修 runner 与 driver 的 gdiags/gwarns 契约

## 非目标（登记不顺手做）

- E8110 类型门全量 / E8120 签名级 / E8170 子回写 / E8180 key Eq——需 sem 用户类型联动
- E8191/E8192 布局静态分析——需样式解析（gui_lower 样式表常量化，SL-7）
- 骨架 IR 数据结构（GuiNode/Slot）——SL-7 三产物时引入，本片仍走文本 IR dump
- 检查面移入默认 check 路径——等 C 宿主 sem 对 gui_ck 助手 E2020 分歧解决（W3 登记）

## 验收

- e8_corpus 全绿（pos/neg/warn 三类）；阶梯 24/24；suite 73/73
- 独立 .ctml：语料同码双形态（内嵌/独立）各验一组
- 每片全绿即落库（pathspec 限定 compiler/src/gui_parse.ct + driver_check.ct + tests/gui/e8_corpus）
