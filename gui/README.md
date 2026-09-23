# gui 域包 —— Ctron 窗口应用运行时(T3 平台面)

clay + raylib 窄桥、CTML 视图解析、域运行时与 headless 验收驱动的单域集合。
**消费形态 = 顶层命名空间 `use gui.{...}`**(2026-09-23 起;域目录在仓库根与
`std/` 平级,分层与门禁见 `std/README.md` 域包章)。

## 布局

```
gui.ct            门面:入口 API + 运行时 + 解析 + 驱动器(消费面唯一入口)
gui/parse.ct      CTML 解析(gt_* 词法/树构建;SL-8a 表达式槽捕获)
gui/theme.ct      主题令牌面
gui/bind/raylib.ct  extern 窄桥(#[trusted];用户不接触)
gui/c_src/        C 胶水单一真源(ctron_gui.c 等;native 链接面)
```

## 快速上手(双口径)

```ct
use gui.{run_kb_d, test, d_click, d_frame, d_type_char, d_expect_text}

struct Model { var count: I32 }

fn inc() { }                       // on:click 处理函数声明(E8120)

fn bind_all(buf: List[Str], m: Box[Model]) {
    if buf[0] == "count" { buf.push("i:" + m.count.to_string()) }
}

fn act(name: Str, m: Box[Model]) {
    if name == "inc" { m.count = m.count + 1 }
}

fn main() -> I32 {
    var src: Str = read_file("app.ctml")       // 或 ctron_embedded()(内嵌 view)
    var m = Box[Model](Model { count: 0 })
    if env_get("CTRON_GUI_HEADLESS") != "" {
        return test(src, 320, 640, |buf| bind_all(buf, m),
            |name| act(name, m), |k| k, |t, a, key| { /* 断言脚本 */ })
    }
    run_kb_d("app", 320, 640, |buf| bind_all(buf, m), |name| act(name, m), |k| k)
    return 0
}
```

```ctml
// app.ctml —— 视图与样式
view Counter {
  <vbox class="root">
    <label class="t">count: {count}</label>
    <button class="b" on:click={inc}>+1</button>
    <spacer class="gap"/>
  </vbox>
}
style root { direction: column gap: 8 padding: 16 }
style t { fg: "#ffffff" size: 24 }
```

## CTML 能力面

- 元素:`vbox` `hbox` `label` `button` `input`(自闭合) `when` `each` `spacer`(自闭合);
  样式 `style <名> { 属性… }` 按 class 关联。
- **表达式绑定(SL-8a/8b)**:槽内容为 Ctron 表达式,规范形 = 去空白原文——
  `{draft + "!"}` 叶串接、`cond={n > 0}` 比较、`{n * 2}` 算术、`{form.draft}`
  点链;求值器文法 = 比较/加减/乘除模/负号/路径(`.len` 取长),值带型通道
  (`i:` 整 / `b:` 布 / 裸 = 串)。**纯读门 E8190**:槽内赋值/自增减即拒;
  `on:` 事件槽豁免(事件期求值,当前为处理函数名单形态,表达式事件随 SL-8c)。
- bind 通道:每帧按槽名询问用户 bind 闭包(`buf[0]` 分发,答案 push);
  `when:名`→"1"/"0"、`each:名`→逐项 push 为域保留前缀。
- 已知限制:叶字面量含标点在内嵌形态会被重建插空格(避用);input 只渲空盒,
  回显靠 mirror label;视口须容得下全内容(Clay 剔除视口外渲染命令)。

## 入口 API(四入口语义,无重载故乘四)

| 入口 | 源 | 键盘 | 用途 |
|---|---|---|---|
| `run(src, title, w, h, bind, act)` | 显式 | 无 | 窗口(纯鼠标) |
| `run_kb(src, title, w, h, bind, act, key)` | 显式 | 有 | 窗口(键入) |
| `run_d(title, w, h, bind, act)` | 默认锚 app.ctml(缺失回落内嵌) | 无 | 窗口 + 热重载环 |
| `run_kb_d(title, w, h, bind, act, key)` | 同上 | 有 | **窗口 + 键入 + 热重载(最常用)** |
| `test(src, w, h, bind, act, key, script)` | 显式 | 注入 | **headless 断言(强制无窗)** |
| `test_sk(tree, …)` | 骨架直通(gui_sk_load) | 注入 | headless(编译期骨架口径) |

热重载环:60 帧内容比对 → gt_parse 原址重解析 → 树换源,闭包状态天然保留。

## 驱动器(headless 断言面)

`d_frame`(重绘)、`d_click`(按处理函数名点击)、`d_type_char`/`d_press_key`
(注入键)、`d_expect_text`/`d_expect_absent`(文本断言)、
`d_cmd_count/type/text_len/text_byte/x100/y100/w100/h100`(命令缓冲逐条访问,
黄金帧差分与几何断言底座)。

## 坑位(实证登记,续接必读)

- 跨包符号**必须显式请求**:use 面缺什么,发射面就缺什么(选择性合并契约)。
- headless 视口给足(建议高 640):内容超视口被 Clay 剔除,断言面即"消失"。
- input 回显写 mirror label(`{draft}`);`{ident}` 简单槽走名字通道,
  表达式槽走求值器——两态并存零破坏。
- 零参方法(`.pop()` 等)发射面缺口:以重建列表 + 成员赋值配方绕行。
- 域包形态构建:`CTRON_STDPATH` 指向 std 三级解析根(域根随仓库布局解析),
  native 链接 `gui/c_src/ctron_gui.c` + `vendor/gui/build/libraylib.a`。

## 门禁与活样例

- 阶梯 `sh tests/gui/run.sh`(30 夹具:s1–s26 + e8 语料 22 件);`ci.sh [8/9]` 挂载。
- 示例:`examples/todo`(键入/列表/空态全链)、`gui_counter`(最小活模型)、
  `gui_calc`(全场景断言)、`gui_cjk`(中文渲染);各目录 `run.sh` 直跑,
  `--run` 开真窗口。

## 分层与稳定口径

T3 平面面(依赖窗口/GPU):**永不进 std 通用门禁**;泳道自有 headless 命令
缓冲验收为准。vendored C 在 `vendor/gui/`(clay/raylib),包内 `c_src/` 只放
自写胶水。设计文档:`docs/superpowers/specs/2026-09-16-gui-ctml-design.md`
(规范)、`docs/superpowers/specs/2026-09-19-gui-use-gui-design.md`(域包设计)。
