# S1 Val 标量迁移——续接提示词

> 用法:新会话开场说「按照 docs/superpowers/plans/2026-09-27-s1-val-migration-prompt.md 继续」,
> 本文档自包含全部上下文/门禁/坑位,无需会话史。

## 你是谁、做什么

你是 Ctron 编译器线的实现工程师。当前任务 = **S1:解释器值表示从字符串编码 List
迁移为 Val 结构体**(eval 值/流表示换代 v1 的 S1 期,设计全文见
docs/superpowers/specs/2026-09-26-eval-val-struct-design.md,必读)。
预算 2-3 日。每步有门,门不过不进下一步。

## 基线(开工前必核)

- 远端 main CI 九门绿(最新 run 见 gh run list);
- 解释器 loop 基准 = **810 次 arena 分配/迭代**(CTRON_AMEM=1 实测);
- suite 自举 73/73;smoke 与基线等价;
- 我名下零未落库件(多泳道并行,开工前 git 重对齐)。

## 必读文件(按序)

1. docs/superpowers/specs/2026-09-26-eval-val-struct-design.md —— 设计全文
   (Val/Entry/Flow 三结构、分期 S0-S3、验收表)
2. docs/superpowers/plans/2026-09-25-eval-repr-overhaul.md —— 归因/分期/坑位
   (Phase 0 归因、Phase 1 缓行判定、conc×3 根因终版、eprint/list_set 提案)
3. compiler/src/eval_val.ct —— 值核心(1,636 行;vI/vB/vS/v6/vV2 构造器、
   fmt/eq_val/vcmp、c6 族、txt_num/dvi)
4. compiler/src/eval_run.ct —— 语句执行(454 行;run_block 的 drop 循环
   ~L395、while handler ~L230)

## S1 范围(设计 §S1)

Val struct(tag/iv/b/s 四槽)替换标量值的字符串编码:
- 构造器 vI/v6/vB → Val{tag, iv, b, s:""}(零 arena 分配);
- w_arith(c6 快路径已做 int64 字段运算,迁移后走同一快路径);
- env 条目 [名, Val] 二元子列表 → Entry struct(名 + Val);
- Flow e4/s4 → Flow struct(kind/env/val/out),e4 元组消除。
复合值(U/T/D/CH/BOX 等)沿 Val.s 串编码位面不变(零迁移)。

## 执行序(每步独立可回退,门不过即 revert)

### Step 0:worktree 沙箱 + 基线画像

```
git worktree add /tmp/s1-val HEAD
cd /tmp/s1-val && sh compiler/build.sh && sh compiler/native.sh
export CTRON_STDPATH=/tmp/s1-val/std
CTRON_AMEM=1 ./compiler/bin/ctron-cc run selfhosted/bench/loop.ct
# 记录:AMEM count / RSS / real(基线画像)
```

- 门:构建 rc=0;AMEM count ≈ 810 万(10K 迭代基线)。
- 坑:docker 复刻 CI 时挂载目录的 macOS 构建产物必须先 Mask/重建,
  否则 Exec format error 假红连环(坑位 #23)。

### Step 1:Val struct + 标量构造器迁移

- eval_val.ct 加 struct Val{tag:I32, iv:I64, b:Bool, s:Str};
- vI/v6/vB 改返回 Val(调用方 ~50 站点同步);
- 门:suite 自举 73/73 + neg/arith 探针电池(见回归网)+ AMEM 对账;
- 坑:同行者并发编辑同一文件——**改动即 git add+commit,防被清扫**
  (坑位 #23;本 session 实证:未提交编辑被同行者树操作吞掉 ≥3 次)。

### Step 2:算术/比较字段化

- w_arith I32/I64 路径 + c6 快路径改 Val 字段运算(消 parse/format);
- vcmp/eq_val 标量分支改字段比较;
- 门:同 Step 1 + fx_val_negarith 双臂逐字 + fx_val_panic_order 三钉。

### Step 3:env/Flow 结构化

- env 条目 [名,Val] → Entry struct;e4/s4 → Flow struct;
- 坑:共享树并发编辑(坑位 #23)+ while-dedupe 交互(计划 Phase 1 注);
- 门:suite 73/73 + smoke 全量 + AMEM loop ≤ 200/迭代(≥4×)。

### Step 4:落地

- 每步单提交;全部门过后整链 push;
- CI 九门绿 = 收官;push 后 conc 三件/fmap/crypto 回归全跑一遍确认。

## 回归网(探针电池,全部双臂对拍)

| 探针 | 钉什么 | 位置 |
|---|---|---|
| fx_val_panic_order | panic 消息/Drop 逆序/时序三钉 | compiler/test/ |
| fx_val_negarith | 负数/域内边界算术(I32 溢出=trap 非折回) | compiler/test/ |
| /tmp/bomb_repro.ct | while 内 assert+Drop 最小复现 | 会话史重建 |
| neg2/arith | 基础负数/正数算术 | 会话史重建 |

## 坑位清单(本 session 实证,全踩过)

1. **自举无中缀 as**:只有方法形态 x.as[T]();写 (b as Str) → seed 解析
   E1001 → native.sh 静默失败 → bin/ 留旧版 → 跑验证=跑旧二进制
   (假绿假红连环)。插桩/改动后必 strings bin/ctron-* \| grep 标记验证。
2. **闭括号裸写**:Ctr 字符串里 `\{` 开、`}` 闭(裸写);`\}` 在 C 宿主
   lexer 非法 → seed 拒绝 → 同上静默失败。本 session 踩 4 次。
3. **共享树并发编辑**:多泳道同写一文件,git add 提交的是整文件——
   同行者的半成品会被扫入。同文件混合编辑期用 hash-object+update-index
   提交索引版,或改用 worktree 沙箱。
4. **同行者树操作清扫**:peer 的 checkout/clean 会吞掉共享树上所有
   未提交编辑(本 session ≥3 次)。改完即提交,不留未提交窗口。
5. **stderr 分流**:2>&1 会把 atexit 转储/panic 消息并进发射产物——
   诊断输出与程序输出必须分流捕获。
6. **emit 不跑语义**:ctron-emit 不执行语义检查,未解析名称静默过 C
   编译(gcc14 隐式声明=硬错才拦)。sem 问题的症状=C 链接错而非 E2020。
7. **docker 复刻 CI**:挂载目录的 macOS 构建产物必须 Mask/重建;
   docker.io 拉取可能 EOF(网络);Docker Desktop 可能掉 daemon(open -a Docker 重启)。
8. **平台差异**:conc×3 的 drop 时机/消息流差异 = linux-x86_64 专属,
   mac arm64 不复现——linux 问题必须在 linux 容器/CI 上验证。

## 回退

每步单提交;门不过 = git revert 单提交,回到上一步基线,重分析再进。
全链回退 = git revert S1 全部提交,回到 c6 快路径基线(810/迭代,已比
初始 20,767 好 25×,豁免面维持现状)。

## 关联

- 设计全文:docs/superpowers/specs/2026-09-26-eval-val-struct-design.md
- 归因/分期:docs/superpowers/plans/2026-09-25-eval-repr-overhaul.md
- 内存债条目:ctron-release-v001-state.md 的「优化线收官」段
- 坑位详表:ctron-compile-driver-gotchas.md #19-#25
