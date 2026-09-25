# GUI 组件库波次五a实施计划:图像/纹理管线(§2.6;波次四被 SL-8c-4 阻塞故前置)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans。Steps 用 checkbox 跟踪。
> 规格:`docs/superpowers/specs/2026-09-24-gui-widgets-design.md` §2.6;上游波次一/二/三已落库(b507dbf)。

**Goal:** `<image>` 能力性容器——src 加载、路径键 LRU 纹理缓存(16 槽)、Clay IMAGE 命令 + flush DrawTexture 分支、失败占位框、headless `d_cmd_img` 断言面、s35_image 夹具。

**Architecture:** C 侧纹理槽表(路径键,LRU 计数淘汰,Texture2D 句柄);`gui_image_cfg(handle, wmode, wval, hmode, hval)` = ConfigureOpenElement 带 `.image.imageData = &tex`(w/h 缺省时 Ctron 经 `gui_image_dims` 查纹理原尺寸定 FIXED);flush 增 IMAGE 分支 = DrawTexturePro 拉伸入 boundingBox;加载失败槽 tex.id==0 → 不设 image 配置,纯底色框即占位。命令类型 3=IMAGE 进 gui_cmd_type。

**Tech Stack:** gui.ct + ctron_gui.c + tests/gui/s35_image(资产 = 夹具内 4x4 PNG,python 生成入库;read_file 形态绕编译器白名单——overlay 先例,白名单登记缓行)。

## Global Constraints

- pathspec 提交;动工前 git 重对齐(peer gui_parse/parse_pkg 在飞,编译器面禁触)。
- 门禁:阶梯 34/5 存量 + s35 新绿 + tests/net;FT_OFF 口径不变。
- 缓存:16 槽 LRU(纹理大于字形,槽小于 ft_shim 64——规格「仿先例」口径,README 注记)。
- 失败口径:加载失败渲占位框不崩(规格 §2.6);占位=底色框(bg 令牌或灰)。

---

### Task 1: C 纹理面

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(纹理槽表+`gui_image_open(path)->handle`(命中续用/载入/失败 id=-1 落占位槽)+`gui_image_dims(handle)->packed w*10000+h`+`gui_image_cfg(handle,wmode,wval,hmode,hval)`(ConfigureOpenElement 带 image.imageData)+flush IMAGE 分支(DrawTexturePro)+gui_cmd_type 增 IMAGE=3)

**Steps:**
- [ ] 六件落地;头文件区 Texture2D 引入(raylib.h 已含)
- [ ] s31/s33 回归(零触碰路径)
- [ ] 提交

### Task 2: `<image>` 元素 + s35 夹具

**Files:**
- Modify: `gui.ct`(白名单 is_img 链;`src` 属性解析→npre 惯例位;rt_emit image 分支:handle=gui_image_open(npre)→w/h 样式或纹理原尺寸→gui_image_cfg;失败占位=同框无图)
- Create: `tests/gui/s35_image/`(test.png 资产 4x4 红块 python 生成;run.sh 仿 s34;断言:type=3 命令存在+几何=样式盒+缺失 src→无 type3 有占位框+双 frame 缓存命中(同 handle——第二次 open 不再 LoadTexture,trace 口径)+w/h 样式拉伸)
- Modify: `tests/gui/run.sh`(追加)

**Interfaces:**
- Produces: `<image src="..." class>` 元素;命令类型 3=IMAGE;d_cmd_img 访问器(P2 图标/头像消费)
- Produces: 纹理缓存 LRU 16 槽(图标族共享)

**Steps:**
- [ ] T1+T2 落地;s35 全绿;全阶梯回归
- [ ] 提交

### Task 3: 收尾

**Steps:**
- [ ] README: image 用法/缓存口径/失败占位;规格 §2.6 落地注记(16 槽)
- [ ] net 门禁;记忆更新
- [ ] 提交

## Self-Review

- 规格覆盖:§2.6 全项(加载/缓存/失败占位/尺寸/headless 断言);图标/avatar/图表为下游(P1 目录在册)。
- 编译器触碰:零(read_file 形态;白名单缓行与 overlay 同口径)。
- 无占位符;资产生成方式明确(python zlib PNG)。
