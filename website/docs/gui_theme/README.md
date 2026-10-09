# gui_theme 包 —— 主题面(gui.ct 拆分阶段一)

Theme struct(25 字段 = 槽序)+ theme_default/apply + 平台探测三件 + 内置
八主题×2 族(平台×明暗缺省)+ theme_contrast + 令牌解析
(`gtok_*` / `g_parse_color` / `g_parse_alpha`)。**零依赖**:不引用 GuiTree /
rt_emit / 编辑管线 / 解析器——纯数据 + 纯函数。

**消费形态 = `use gui_theme.{...}`**;gui.ct 门面原样 re-export,消费面
`use gui.{...}` 不变。

参考页:[gui_theme.md](gui_theme.md)(签名面自动生成)。

主题的「品味」分发形态(主题 = 包)见 [themes 包](../themes/README.md)。
