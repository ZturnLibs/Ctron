# 泳道分工与协作协议(Lane Split)—— 与自举 session 对齐

> 创建:2026-09-05。对应 `2026-09-05-c-bootstrap-HANDOFF.md`(自举 session 的交接文档)。
> 背景:两 个(或以上)session 并行工作于同一仓库同一分支。曾发生未提交改动被
> `git restore` 清掉、目录迁移竞态等。本文钉死分工与协议,双方遵守。

## 1. 泳道划分

| 泳道 | 归属 | 范围 |
|---|---|---|
| **A:自举** | 自举 session(见 HANDOFF) | `selfhosted/` 全部(C9b 求值器扩面、C9c cc 驱动、后续模块)、`suite_run`/`suite_diff` 夹具、`compiler_c/src/rt.c` 的**宿主内建新增**(read_dir 等,按需) |
| **B:C 版纵深** | 本 session | `compiler_c/src/{sem,sem.h,pkg,pkg.h,main,parser,lexer,ast_show}.c`、**代码生成后端**(trans/build)、CLI 工具链、诊断注册表补全;`tests/` 一致性语料的增补 |
| **C:Rust 参照** | 暂缓 | `compiler/`(P1-D WIP 已在库,双方均不动,除非明确领任务) |

## 2. 协作协议(硬规则)

1. **只碰自己泳道内的文件**;跨泳道修复允许,但必须:(a) 最小 diff;(b) `make test` 全绿;
   (c) 提交信息说明动机(例:a3bf0b7 修 rt.c List 索引写/byte_at UB)。
2. **compiler_c/src 是共享 oracle**:语义改动 = 改变 oracle,须在提交信息首行标注
   `oracle-shift:` 并在 selfhosted 侧复核差分;纯 bugfix(对齐既有语料行为)不需标注。
3. **不 restore / 不 rebase / 不 stash 他人的未提交改动**。发现冲突文件:跳过该文件,
   在提交信息中注明,或等对方提交后 rebase 自己的工作。
4. **小步快提交**:编辑→构建→验证→`git add <具体文件>`→commit,压缩未提交窗口
   (本轮已发生两次未提交窗口事故)。
5. **里程碑代号分段**:A 泳道沿用 C9x;B 泳道用 **C10x(后端)/C3x(语义)延续**,
   避免编号相撞。
6. 每次交付后更新本文与 `compiler_c/README.md` 里程碑表(各自只改自己的行)。

## 3. 当前状态快照(2026-09-05)

- B 泳道已交付:C1/C2 前端、C3-a…f 语义(21/23 注册表码)、C4-a…i 解释器(34/34 语料)、
  C5–C7 自举前端差分、C9 工具链(pkg/--format=json)。
- A 泳道已交付:selfhosted 词法/树解析/文本解析/sem_chk/pkg_chk/ev_num(见 HANDOFF §2)。
- B 泳道下一里程碑:**C10-a Ctron→C 转译后端骨架**(数值域:宽度全集/检查算术/
  控制流/函数递归/test 块;差分对象 = C 解释器 `ctronc run`)。
