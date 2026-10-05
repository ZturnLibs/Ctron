# G 档(GUI-32~44)收尾交接——2026-10-04 深夜

## 状态总览:11/13 落库,2 件收尾中

| 件 | 提交 | 备注 |
|---|---|---|
| GUI-40 palette | e9ec0f41(经 7a9aebcf) | ✓ |
| GUI-38 date-time | eddca1c6 | ✓ |
| GUI-39 color | 775bc6cf | ✓ |
| GUI-42 动画库 | 58fc148f | ✓ |
| GUI-41 chart v1 | c3318009(含多 canvas 能力) | ✓ |
| GUI-43 chord | 69760e9c(经 aa4e98db merge) | ✓ |
| GUI-37 内部拖放 | 8d930b80(f1d2ac52 merge) | ✓ |
| GUI-44 异步→UI | a88dbe41(84f7dcea merge) | ✓ |
| GUI-34 RTL | 2221d308 | ✓ |
| GUI-33 富文本 v1 | 231c63da + **94879efe 补遗(TEXT fg 包装三件,原漏 add)** | ✓ |
| GUI-36 触摸/手势 | c1af7906 | ✓(已合 main) |
| GUI-35 无障碍 v1 | **未落库**——gui35-a11y3 worktree:a11y_role_of+d_a11y_dump 已写进 pkgs/gui/gui_render.ct(未提交)、facade re-export 已改、tests/gui/s90_a11y 夹具已复制、阶梯已注册;s90 首跑 emit 一次段错误一次过(bin 陈旧疑),**未验完** | 收尾中 |
| GUI-32 虚拟多窗 | 未动 | 待做 |

主树 HEAD(最后确认)= 0ab88aae(main Merge gui33-richtext;含 16a40a4f 对端 ctcl 修复与
35bd03f6 对端 t31-m15 发射合流)。阶梯主树验证 90 过/1 败(s30 在册)。

## 中断原因(诚实记录)

会话 shell 的 cwd 指向被 `git worktree remove --force` 删除的嵌套目录
(.worktrees/gui37d/.worktrees/gui35 一族),spawn 全部 ENOENT,Bash 通道卡死;
Write/Read 工具仍可用。已无害重建多个候选路径(.cwd-recovery 标记文件,可删)。
恢复=新会话/新 shell(任意有效 cwd)即可,仓库无半成品冲突:
主树工作区干净(唯一未提交改动=无;gui35 的 gui_render.ct 改动在
.worktrees/gui35 工作树内,未提交)。

## 收尾步骤(GUI-35,预计 30 分钟)

1. `cd /Users/zyj/Zturn/Ctron/.worktrees/gui35`
2. `cp /Users/zyj/Zturn/Ctron/tests/gui/s90_a11y tests/gui/ -r`(若缺)
   + 阶梯注册行(s90_a11y)在该 worktree run.sh 已改(未提交,丢失则重加一行)
3. `cp /Users/zyj/Zturn/Ctron/compiler/bin/ctron-emit ctron-cc compiler/bin/`
   + `cp ~/.auth 0` 不需要;`sh compiler/build.sh && sh compiler/native.sh`
   (对端 pkg.c 落库后 seed 必须重建——s23 总线错教训)
4. `sh tests/gui/s90_a11y/run.sh` 双跑(emit 段错误一次→复跑在册口径)
5. 全门(阶梯/suite/smoke/双陈列室)→ 落库 → 合 main
6. COVERAGE 条目模板已在 tests/COVERAGE.md 尾部?否——本文件即模板源
   (role 推导/叶文本/bind 路径三断言;mac shim 另案在册)

## 收尾步骤(GUI-32 虚拟多窗 v1,1-2 天)

- 规划=2026-10-04-gui-g-tier-plan.md §3 GUI-32(引擎级浮板,0924 替身红线:
  API 命名 gui_board_open 类,文档明示引擎级)
- 落点=gui_driver.ct 窗口循环面扩展(每浮板独立 GuiTree+rects+焦点;事件路由
  焦点板优先);夹具 s91_win(headless 双板路由/关闭/z 序)
- 门同惯例;真窗 --run 手验待用户

## 全档收官清单

- 路线图勾销:2026-10-02-gui-remaining-roadmap.md G 档 13 条改 ✅(35/32 注明 v1 边界与另案项)
- 记忆:ctron-gui-lane-state.md 续37(G 档 13/13;本次会话 shell 卡死经过一句)
- 本 handoff 文件归档或删除

## 本档新增在册坑(合并入泳道记忆)

- 嵌套 worktree 的 cwd 删除会卡死会话 shell(worktree remove 前必先 cd 出)
- gui_now_ms 时钟域跨越(注入前 ts=真时钟,注入后为负永不 due)
- pub extern 带参 E2020.use 未找到(无参可)——包装 fn 绕行
- 触摸/drag 类 fire 后 cmds 槽滞后一帧(双帧断言)
- 绝对/相对路径 emit 差异=烟雾弹(真因多为本地裸头 0 参生成)
