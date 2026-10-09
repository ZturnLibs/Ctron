# gui_widgets 包 —— 组件库(gui.ct 拆分阶段三)

16 组件全量规范形态:Select / WList / Dialog / Menu / EditMenu / MenuBar /
List / MultiList / Slider / Progress / Tabs / Badge / Accordion / Tooltip /
Toast / Table。

**纯 prop 契约**:组件体仅消费自家 prop 与内建名(`dragpx`/`mods`/`fld`),
宿主模型零耦合;事件处理器驻留宿主(名字扁平视图解析,合约Ⅱ-b 模型捕获
直传)。宿主按需定义处理器(节选):

- Select: `sel_toggle` / `sel_pick`
- Dialog: `dialog_close` / `dialog_cancel` / `dialog_confirm`
- WList: `list_pick`
- Menu: `menu_toggle` / `menu_pick`

**消费形态 = `use gui_widgets.{...}`**。

参考页:[gui_widgets.md](gui_widgets.md)(签名面自动生成)。
