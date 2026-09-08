# 泳道分工与协作协议(Lane Split)—— 与自举 session 对齐

> 创建:2026-09-05。对应 `2026-09-05-c-bootstrap-HANDOFF.md`(自举 session 的交接文档)。
> 背景:两 个(或以上)session 并行工作于同一仓库同一分支。曾发生未提交改动被
> `git restore` 清掉、目录迁移竞态等。本文钉死分工与协议,双方遵守。

## 1. 泳道划分

| 泳道 | 归属 | 范围 |
|---|---|---|
| **A:自举** | 自举 session(见 HANDOFF) | `selfhosted/` 全部(C9b 求值器扩面、C9c cc 驱动、后续模块)、`suite_run`/`suite_diff` 夹具、`compiler-c/src/rt.c` 的**宿主内建新增**(read_dir 等,按需) |
| **B:C 版纵深** | 本 session | `compiler-c/src/{sem,sem.h,pkg,pkg.h,main,parser,lexer,ast_show}.c`、**代码生成后端**(trans/build)、CLI 工具链、诊断注册表补全;`tests/` 一致性语料的增补 |
| **C:Rust 参照** | 暂缓 | `compiler/`(P1-D WIP 已在库,双方均不动,除非明确领任务) |

## 2. 协作协议(硬规则)

1. **只碰自己泳道内的文件**;跨泳道修复允许,但必须:(a) 最小 diff;(b) `make test` 全绿;
   (c) 提交信息说明动机(例:a3bf0b7 修 rt.c List 索引写/byte_at UB)。
2. **compiler-c/src 是共享 oracle**:语义改动 = 改变 oracle,须在提交信息首行标注
   `oracle-shift:` 并在 selfhosted 侧复核差分;纯 bugfix(对齐既有语料行为)不需标注。
3. **不 restore / 不 rebase / 不 stash 他人的未提交改动**。发现冲突文件:跳过该文件,
   在提交信息中注明,或等对方提交后 rebase 自己的工作。
4. **小步快提交**:编辑→构建→验证→`git add <具体文件>`→commit,压缩未提交窗口
   (本轮已发生两次未提交窗口事故)。
5. **里程碑代号分段**:A 泳道沿用 C9x;B 泳道用 **C10x(后端)/C3x(语义)延续**,
   避免编号相撞。
6. 每次交付后更新本文与 `compiler-c/README.md` 里程碑表(各自只改自己的行)。

## 3. 当前状态快照(2026-09-05)

- B 泳道已交付:C1/C2 前端、C3-a…f 语义(21/23 注册表码)、C4-a…i 解释器(34/34 语料)、
  C5–C7 自举前端差分、C9 工具链(pkg/--format=json)。
- A 泳道已交付:selfhosted 词法/树解析/文本解析/sem_chk/pkg_chk/ev_num(见 HANDOFF §2)。
- B 泳道下一里程碑:**C10-a Ctron→C 转译后端骨架**(数值域:宽度全集/检查算术/
  控制流/函数递归/test 块;差分对象 = C 解释器 `ctronc run`)。

---
## 战略转向(2026-09-05 晚)—— Rust 版成为唯一产品线
项目方决定:**不再推进“Ctron 自举”(Ctron-in-C);C 版(compiler-c)与 selfhosted 仅保留为
参考 oracle / 归档,不再新增功能**。**Rust 编译器成为唯一实现线**:
- 目录:`compiler/` **已改名 `compiler-rust/`**(本次提交)。包名仍 `ctron`。
- 归属:**Rust 线独立全力实现**(单 session 全权),其它泳道/其它 session **不得再修改
  `compiler-rust/`**(旧“C:Rust 参照—暂缓”条款作废)。
- C 版纵深(C10-a trans 等)与 LSP/editors 泳道:各自现存内容保留,不作为本项目主线。
- 后续里程碑代号改用 **R- 前缀**(例 R-P1D、R-P2…),与历史 C*/P1* 区分。
- 验收盘:`cargo test`(lex/parse/check/run 四 suite)+ 61 文件行为矩阵(目标全绿),对照
  `docs/spec/` 与 `tests/` 语料。

## 附:C10-h trait 参数单态化设计预案(2026-09-05 调试结论,实现时避免重蹈)

目标:07_capabilities(`elapsed_since(clock: &Clock, start)` + 调用点传 FakeClock)。

已验证的设计要点:
1. **decl_ty_tc 需 traits 表**:D_TRAIT 名单收集;`&Clock` 参数 → TY_REF 剥壳 → `Clock` TY_NAMED
   查 traits 表 → T_TRAIT(tname=trait 名)。subs 检查必须在 TY_NAMED 守卫内、prelude 之后
   (&Clock 是 TY_REF,subs 循环只匹配 TY_NAMED,靠 TY_REF 分支递归 sub 后再查)。
2. **emit_fn 签名改 cdecl***:`static void emit_fn(tc* c, const cdecl* d, const char* cname)`
   + 体内 `const cfn* F = &d->fn_;`。调用点传 `d`(非 `&d->fn_`!传 cfn* 会让 ensure 内
   `d->fn_` 二次偏移读垃圾 → 空体 void 函数)。
3. **特化发射必须走临时缓冲**:`c->out_sb = &tmp` + emit_fn + 完成后 `sb_s(&m_sb, tmp.d)`。
   直接写 m_sb 会让嵌套 ensure_method(如特化体内的 `clock.now()`)把方法体插进
   未闭合的特化函数中间(非法 C)。
4. **调用点**:argtys[i].k == T_TRAIT 且实参 aty 具体 → 特化键 = `__<Mangle(aty)>`;
   首个 trait 参数驱动;subs = {from: trait 名, to: 实参 ty}(引用实参 AST 名字安全,
   ty 值拷贝)。
5. **主发射循环跳过 monoed fn**(否则泛型原体先于特化体发射,其体内 trait 参数
   分派失败即 c->err 置位,整个 trans 失败)。头部原型同步跳过。
6. **陷阱实录**(本轮全部踩过):ensure 去重早退未设 out_ret(垃圾类型);
   `*out_cname = <指针>` 写入调用者 char[192](应 snprintf 拷贝);scope_pop 未递减
   槽位(64 次后作用域互串);emit_fn 未恢复 fn_ret(in_test 泄漏致方法体裸 return)。

验收:07 原生执行 = 解释器;其余 17 语料回归全绿。
