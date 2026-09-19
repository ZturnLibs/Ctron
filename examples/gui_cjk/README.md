# gui_cjk —— Ctron GUI 中文一等示例

FreeType 中文纹理渲染 + 真实点击计数:标题「Ctron 中文窗口」为静态纹理,点击蓝色
圆角按钮后 `count: N` 纹理即时重渲——演示 GUI 域「中文一等」目标(M3 文本管线第一片)。

这是**直绘路径**(raylib 纹理自持坐标,不经 Clay 布局);声明式路径(CTML + Clay)见
`examples/gui_counter`,Clay TEXT → FreeType 的渲染集成随 M3 落地后两条路径合流。

## 运行

```sh
sh run.sh          # 构建 + headless 渲染冒烟(无 CJK 字体则 skip)
sh run.sh --run    # 追加启动真实窗口,真实点击交互
```

## 文件导览

- `src/main.ct` —— 入口:`CTRON_GUI_HEADLESS` 环境变量切换 headless 冒烟 / 窗口循环;
  字体探测(macOS Hiragino/PingFang/Songti,Linux Noto/WQY)在 shim 内
- `c_src/ft_shim.c` —— FreeType 两遍渲染(测量/合成)→ RGBA → raylib 纹理注册表
  (同步自 tests/gui/s9_window_cjk,增量 `ft_last_w/ft_last_h/ft_probe_nonzero`
  供 headless 冒烟读回;阶梯修复时随 PR 手动同步)

## 当前能力边界(如实)

- headless 冒烟在无 CJK 字体的环境下 skip(rc=0)——逐字节严格断言归 `tests/gui/s8_cjk`
- 无 IME 组词(preedit)——M0–M2 定义形态;composer 随 M3
- shaping(HarfBuzz)/混排退绕随 M3 后续切片
- 解析器/绑定与本示例无关(直绘路径);严格验证归 `tests/gui` 阶梯(s8/s9)
- `c_src/` shim 为**过渡形态**(GUI 规范 §11.2 增补):bind 层(MVP 阶梯 W6)落地后
  删除——目标应用源码树零 C;用户写 C 仅剩第三方接入逃生口场景
