# themes 包 —— 主题作为可导入包(GUI-31)

主题 = 包,不进引擎。内置 theme_dark/light/mac_*/win_*/linux_* 族(平台×明暗
缺省)保持最小核(住 gui.ct);**品味主题经本包分发**——每主题一枚 pub fn
构造全套 gui.Theme(25 字段),应用 use 后一步换装:

```ct
use gui.{theme_apply}
use themes.{nord, dracula}
```

参考页:[themes.md](themes.md)(主题构造函数全表,签名面自动生成)。

主题的结构面(Theme 25 字段槽序)见 [gui_theme 包](../gui_theme/README.md)。
