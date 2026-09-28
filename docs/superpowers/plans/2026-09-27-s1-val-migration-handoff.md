# S1 Val 迁移——第一执行日检查点(2026-09-27 晚)

> 用法:新会话说「按照 docs/superpowers/plans/2026-09-27-s1-val-migration-handoff.md 继续」。

> **进度更新(同日深夜)**:cx 包装表终案已执行完毕并落库——分支
> `s1-val-migration` 提交 ba28f4b(Step1+2 全绿:72/73、smoke 152/0、三金样+探针
> 逐字、seed 臂分歧全消)。AMEM 8,101,659 → **4,600,273**(记账 810→460/迭代,
> 1.76×);门 ≤200 万未达。余量分析见文末「AMEM 剩余构成」。本文件前半部分
> 的「终案待执行」描述已过时,以本更新为准。

> 本文自包含:进度/根因/终案/坑位/复验命令。上游:2026-09-27-s1-val-migration-prompt.md。

## 一句话状态

Step 0/0.5 完成;Step 1 类型面大扫荡已完成并**native 臂全绿**(三金样+三探针金样+suite
72/73 同构 gap);**seed 解释臂发现自举两侧 struct 语义分歧**(设计 §4 预留的坑成真),
根因已破、终案已定(cx 包装表),差最后一轮改造+全门禁,预计半日内收官。

## 沙箱与产物

- worktree:/tmp/s1-val(detached 818c8d5;compiler-c/build 软链主树 seed;**改动未提交**,
  无同行者会清扫 /tmp 沙箱,安全)。
- AMEM 插桩:driver_emit.ct **worktree 本地未提交**改动(ctron_amalloc env 门控计数器,
  CTRON_AMEM=1 时 atexit 打 `AMEM <n>` 到 stderr);**提交时必须排除 driver_emit.ct**
  (黄金发射文本漂移风险),或作为显式单独提交先过用户。
- 基线画像(loop.ct,10×10K 内层迭代):AMEM 总数 **8,101,659**(≈810 万;续接词的
  「810/迭代」= 810万÷10K 记账口径),0.52s,317MB RSS。**门 ≤200/迭代 ⇔ 总数 ≤200 万**。
- 基线门禁:73 件套 **72/73**(behavior 40/41,gap=06f mac/dyn_link CI,在册);smoke
  **152 ok/0 fail**;decl 锁 402(扫荡后 413=+11 新 decl,smoke 有 decl 锁断言需同步)。
- 探针金样(compiler/test/):fx_val_negarith rc=0 出 "true";fx_val_panic_order rc=1
  stdout=`before\nboom\ndrop:1`;/tmp/bomb_repro.ct(上一会话真迹)rc=1
  stdout=`assert failed\nboom`。运行:`bin/ctron-cc run <file>`。

## 已完成(Step 1 主体,全在 /tmp/s1-val worktree)

- eval_val.ct:`struct Val{tag,iv,b,s,cx}`(0=V 1=I 2=6 3=7 4=B 5=S 6=D 7=W 8=复合)、
  vI/vB/vS/vV2/v6/v7/vW/vL 原生化、v_box/v_unbox 容器边界适配、v6t/v7t 文本投影、
  vtag_str、truth/veq/vcmp/eq_val/fmt/fmt_struct Val 化、env_add/env_set 收 Val、
  env_at_val、**Flow struct{kind,env,v,out}+e4/s4 换装**(e4/s4 字段序统一后,
  eval 结果直传即换形)。
- eval_expr/eval_call/eval_run/eval_pat/eval_trait/eval_env/eval_float/eval_width/
  sem_ceval/sem_comptime/gui_parse/driver_run/driver_emit(const 段)全量扫荡。
- 复杂值(U/A/L/T/P/R/C/F/M/BOX/NS/SCOPE/SIMD/VEC/TASK/CH/MUX)旧编码位面零迁移,
  env/实参表边界 v_box/v_unbox;6/7 超 I64 巨型字面量回 s 文本槽。
- 验证:native 臂三金样逐字✓、三探针金样✓、suite 72/73 同构✓;sem_ceval/sem_comptime
  comptime 面(含 E6010/E2040 const 界门)过。

## 根因(本日最重要发现,已实证)

**seed 解释器(compiler-c)与发射臂对「struct 字段中的 List」复制语义不同:**

- 发射臂:C struct 浅拷贝,List 字段=指针共享 → 写透/别名保持(probe_alias native:y)。
- seed 臂:**struct 复制时 List 字段被深克隆**(probe_alias seed:obs.slot[0]="y" 后
  c.slot[0] 仍 "x")——疑 V_ARR 型字段(clone_val 对 V_ARR 递归克隆元素,rt_eval.c:49);
  而 **LIST 槽位(env 条目/元组元素/普通 List 变量)双臂皆引用共享**。
- 后果:Val.cx 直挂复合载荷时,凡 struct 过境(局部赋值/传参/返回)后的
  `recv.cx[i]=…`/`push` 写到克隆体上 → seed 臂 fetch_add(10/10/10 vs 10/15/25)、
  input_cc3 sum_list(12 vs 39)、conc_* 分歧。native 臂全对,故 suite(native)绿。
- 注:rt_eval.c:39 clone_val 源码读起来 V_LIST 字段 `return v` 指针共享,与观测矛盾
  → 唯一自洽解释=struct 字段按声明型别落 V_ARR(深克隆),自由 List 变量落 V_LIST
  (共享)。**不必再纠源码,以 probe_alias 观测为准。**
- 已否决方案:①VREG 全局注册表+句柄——编译器自用 static 在 seed 臂经 seed 全局表
  裸表暴露,被 Ident 处理器 v_unbox 误读(l[0] 冲注册表内容),且 static let List 在
  发射臂有 ct_typeof 裸名缺口(probe3:裸名 .len 走 strlen);seed 臂 static 语义
  整体不可信。②序列化/句柄文本化——分配反增。

## 终案(下一会话执行,预计半日)

**cx 槽改放「单元素包装表」:复合值的 Val.cx = [payload](包装壳),payload 指针
在双臂均共享。**

- 依据:字段深克隆只换壳(新 V_ARR、元素指针照抄),payload 是 V_LIST 值
  (clone_val fallthrough 指针共享)→ `v.cx[0]` 永远是原载荷,原地写双臂一致。
  Val 其余字段(tag/iv/b/s)皆标量,克隆透明。
- 改造点(全部机械):
  1. eval_val.ct:`fn cx_wrap(l: List[Str]) -> List[Str] { var w=List[Str](); w.push(l); return w }`
     构造点 cx 槽 = `cx: cx_wrap(l)`;`fn cx_of(v: Val) -> List[Str] { return v.cx[0] }`。
  2. 全 eval 面 `X.cx` → `cx_of(X)`(~180 站点,regex 同前法;**赋值目标先绑局部**:
     `var cc = cx_of(cont); cc[i] = …`,Assign 的 Index-of-Call 基座不支持)。
  3. v_box 复合支 `return cx_of(v)`(恒等穿透不变);v_unbox 复合支 `cx: cx_wrap(l)`。
  4. 标量 cx 初始化:static 哨兵已被否决(VNIL sem 不认 + static 语义双臂不可信),
     暂用内联 `List[Str]()`(每标量 1 分配);**先测 AMEM**(见下),不达门再做
     「cx 槽仅 tag=8 分配」的懒初始化或四槽 Val(tag8 时 s 存句柄+env 兜底)变体。
- 门(同原计划):72/73 同构 + 三探针金样逐字 + smoke 152/0(**先同步 decl 锁 413**)+
  AMEM ≤200 万 + 基准时间/RSS 复测。
- 收官:每步单提交(pathspec 限定,排除 driver_emit.ct 的 AMEM 插桩或单独提交)、
  push、CI 九门绿、conc/fmap/crypto 回归全跑、记忆更新。

## 复验命令(沙箱内)

```
cd /tmp/s1-val
sh compiler/build.sh
# native 臂:
compiler/ctc.sh emit compiler/build/cc_run.ct /tmp/s1new_cc.c && cc -O1 -w -o compiler/bin/ctron-cc /tmp/s1new_cc.c
./compiler/bin/ctron-cc run selfhosted/input_cc.ct   # 对照 selfhosted/expected/*.out
python3 compiler/test/suite.py                        # 72/73
sh compiler/test/smoke.sh --full                      # 152/0(decl 锁见 smoke.sh)
# seed 臂(分歧复现):
TMP=$(mktemp /tmp/ctron_cc.XXXXXX); sed -e "s|ANCHORINPUT|$PWD/selfhosted/input_cc3.ct|" -e "s|ANCHORPROFILE|full|" -e "s|ANCHORLANG|zh|" compiler/build/cc_run.ct > $TMP && compiler-c/build/ctronc run $TMP
# AMEM:
CTRON_AMEM=1 ./compiler/bin/ctron-cc run selfhosted/bench/loop.ct  # stderr AMEM <n>
```

## 新增坑位(入 ctron-compile-driver-gotchas 候选)

31. 自举两侧 struct 复制语义分歧:发射臂浅(C memcpy)、seed 臂 List 字段深克隆
    (V_ARR);**可变复合值禁止挂 struct 字段直传**,须走 LIST 槽位(env 条目/元组/
    参数),或用包装表保 payload 指针共享。旧代码无此坑因值全是 List。
32. 编译器自用 `static let`:sem 层不认 static 名为可解析绑定(VNIL 实证);发射臂
    裸 static 名上成员调用 ct_typeof 缺口(.len 走 strlen);seed 臂 static 经全局裸表,
    被 v_unbox 类处理器误读。**三面皆坑,编译器源内禁用 static 造全局可变状态。**
33. Assign 的 Index 基座只认 Ident/Member 起点,`f()[i]=x`(Index-of-Call)不支持,
    须先绑局部再写。
34. 旧 chk 二进制自检新源会段崩溃(rc=139)——自检前先确认 bin 与源同代;发射+gcc
    才是类型硬门。

## 关联

- 上游计划:docs/superpowers/plans/2026-09-27-s1-val-migration-prompt.md
- 设计:docs/superpowers/specs/2026-09-26-eval-val-struct-design.md(§4 风险「自举
  自用 struct 工具链成熟度」——S0 只验了可编译,未验别名语义,本日补上)
- 内存:ctron-compile-driver-gotchas.md / ctron-release-v001-state.md

## AMEM 剩余构成(2026-09-27 深夜实测,直方图插桩)

总量 4,600,273(loop.ct 100K 内层迭代 = **46/迭代**):≤16B 字符串 1.90M(19/迭代)、
17-32B(**List 头**,sizeof ctron_list=24B)2.10M(21/迭代)、33-64B(items 数组)0.60M(6/迭代)。

- 字符串 19/迭代:env_set 装箱 to_string ×2;其余为值构造内 byte_slice/concat。
- List 头 21/迭代:标量值构造的 **cx 空表**(~9:vI/vB×6+每语句 vV2×2-3)、env_set
  的 outer+entry+box 三表 ×2=6、dedupe 2、其余 4。

**结构性下限 ≈ 2.3-2.5M(230-250/迭代)**——在「复合值必须挂 struct 字段(seed 深
克隆)+List 槽只收指针」双约束下,标量 cx 空表与 env 持久化重建不可避免。三门
(`200 万`门)到达路径,按性价比排序:

1. **环境串化**(预计 -0.6~0.9M):env 条目 [nm, box] 二元表 → 单串 "nm\x1fbox"
   (concat 一次);env_at 改前缀比对(零分配)+值 byte_slice(读时 1 分配)。
   触面:eval_env 全部 + en[0]/en[1] 调用点(~20 处)。
2. **标量 cx 消灭**(预计 -0.9M):需语言级全局哨兵——static let 被 sem/发射/seed
   三面否决(坑位 #32),正道=能力扩展提案(如 `list.empty()` 内建,双臂返回共享
   不可变空表),需用户裁决后动工。
3. **Flow 的 vV2 复用**:每语句尾 vV2() 的 cx 表——同属 #2。

**门裁决建议**:若 #1 落地后 ~3.5M(350/迭代,2.3×),#2 需语言扩展走裁决;
或接受 4× 目标修正为「实际可达口径」由用户定。勿自行放水。

