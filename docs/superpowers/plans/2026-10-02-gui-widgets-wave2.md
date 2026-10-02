# 组件目录第二波——Menu/MenuBar/List(选中态)/Table/便宜件

> 日期:2026-10-02。前置:交互缝四件(7963ef04)落地后,本波全部为**视图级
> 组合**(examples/gui_widgets 陈列室,同源声明),零引擎改动。

## 组件与配方

1. **Menu**(下拉菜单 v1,内联展开同 Select 口径):触发器 + `when mopen` +
   `each mitems` 行(pick 行级「名:下标」契约);选中项重建列表加 "✓ " 前缀
   (todo 重建配方);act 收菜单。MenuBar = hbox 双触发组合(视觉编排,机制同)。
2. **List**(选择列表 单选):`each litems` 行 + `on:click={list_sel()}`;
   act 重建 litems 加 "> " 前缀(lsel 计数比对)。
3. **Table**(列头 + 行选中):header hbox 两列头 + `each titems` 行
   `{it}`(行文本 act 侧预格式 "名 · 值")+ `tab_pick()` 行选中 "> " 前缀。
   多列数据绑定登记:需表达式下标/字符串切分能力(另册)。
4. **Divider/Card**:纯样式 recipes——divider=2px BORDER 盒;card=ELEVATED
   盒包装(陈列室放演示位)。

## 已知限制(登记)

- pick 实例下标分发为真窗 rt_hit_name 行为;headless 断言以裸名分发(夹具
  惯例,同 s42)。
- Table 多列数据需下标/切分表达式;v1 单数据列 + 列头视觉。
- 菜单 v1 内联展开(推下方内容);overlay 锚定随 v2。

## 验收

陈列室 headless 断言扩展(gui_widgets run.sh):开菜单→项可见→pick→✓ 前缀;
list 行点选→"> " 前缀;table 行点选→"> " 前缀;divider/card 渲染存在。
