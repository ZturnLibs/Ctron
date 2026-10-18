# pkgs/tui —— 终端界面域包(T2 系统面)

终端全屏交互应用:单元格网格渲染、约束布局、键鼠事件、令牌主题、headless 测试驱动。
设计规范:`docs/superpowers/specs/2026-10-09-tui-design.md`(12 项裁决全闭)。

- **档位 T2**:唯一系统依赖 = POSIX libc(termios/poll/TIOCGWINSZ/SIGWINCH/write),
  自写胶水 `c_src/tui_term.c`,零第三方 vendored。
- **零编译器税**:不动 compiler/,全部 C 交互走 `#[trusted] extern` 窄桥。
- **门口径:发射臂单臂**——解释臂用户 extern 须符号宿主内链(ctron-cc 无 tui_* 即
  "extern 符号未找到",W3a 探针在册);纯 Ctron 面(Theme 色编码等,W3d)可 seed 测。
  门:`sh tests/tui/run.sh`(s 阶梯;headless 默认)。

## 当前进度

| 波 | 内容 | 状态 |
|---|---|---|
| W3a | 门面骨架 + 窄桥 + tui_term.c + enter/leave 铁律 + s1 门 | ✅ 本批 |
| W3b | buffer/布局/渲染(diff)+ s2–s5 门 | ✅ 2026-10-18 |
| W3c | 输入事件 + run_app/test + panic 消息重放 + s6 门 | ✅ 2026-10-18 |
| W3d | 主题 + 能力探测三档 + s7 门 | 待做 |
| W4 | pkgs/tui_widgets 交互组件(list/input/statusbar/dialog/progress)+ s8 | 待做 |
| W5 | ctml 运行时版(+W5b/5c 触发式) | 待做 |

## W3a 面

- 生命周期:`tui.enter()`(raw+alt screen+隐光标+鼠标+清屏;非 tty panic 防呆)/
  `tui.leave()`(atexit 兜底双保险);任何退出路径必还原(铁律 §8)。
- 进程级事实(胶水全局,裁决 5;应用数据不落此层):`quit()/quit_requested()`、
  `resized_take()`(SIGWINCH 读即清)。
- 查询:`term_w()/term_h()`(ioctl 回退 80×24)、`wait(ms)`(poll:1 输入/0 超时/-1 错)、
  `is_tty()`、`out(s)`(原始输出桥)。
- 核心类型定案(形状钉死,逻辑随 W3b):`TuiBuf`(SoA 九表)/`TuiTree`(十表)/
  `Event`(I32 码 + 具名修饰槽,enum 换装登记债)/`Theme`(14 字段显式传参)。

## 手测清单(--run,W3c run_app 落地后)

进真终端跑 enter/leave:alt screen 进出、光标隐藏还原、raw 模式按键、Ctrl+C 不退
(W3c)、SIGWINCH 重排(W3c)。headless 门不覆盖这些面。
