# gui 域包 —— Ctron 窗口应用运行时(T3 平台面)

clay + raylib 窄桥、CTML 视图解析、域运行时与 headless 验收驱动的单域集合。
**消费形态 = `use gui.{...}`**;包内速览(布局/能力面/坑位全形)以仓库
`pkgs/gui/README.md` 为单一真源,本页为站点导读。

## 参考页(签名面自动生成)

| 页 | 内容 |
|---|---|
| [gui_parse.md](gui_parse.md) | 迷你词法 + CTML 解析核心 + GUI-27 锚合并 |
| [gui_expr.md](gui_expr.md) | bxv 求值器 + 下标切分 + 事件表达式 + 实参解码 |
| [gui_render.md](gui_render.md) | 交互态 + 渲染分派 + 聚焦 |
| [gui_edit.md](gui_edit.md) | input/textarea 管线 + 菜单右键缝 |
| [gui_driver.md](gui_driver.md) | 窗口循环 + 入口装配 + test/d_* 全族 + on:after |
| [gui_anim.md](gui_anim.md) | 动画面 |

注:门面 `gui.ct`(GuiTree + extern 窄桥 + 运行时辅助)的 iface 投影暂缺
(doc 驱动 use 合并 E5030 与真链分歧,登记债务)——入口 API
(`run_kb_d` / `test` / `d_click` / `d_frame` / `d_type_char` / `d_expect_text` /
`theme_apply` 等)以仓库 README 与 [gui_widgets](../gui_widgets/README.md) /
[gui_theme](../gui_theme/README.md) / [themes](../themes/README.md) 三包页为准。

## 快速上手(headless 双口径)

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

相关包:[gui_theme](../gui_theme/README.md)(Theme + 八主题族 + 令牌解析)、
[gui_widgets](../gui_widgets/README.md)(16 组件规范形态纯 prop)、
[themes](../themes/README.md)(主题作为可导入包)。

验收:永不进 std 通用门禁;泳道 headless 命令缓冲驱动(`CTRON_GUI_HEADLESS`)。
