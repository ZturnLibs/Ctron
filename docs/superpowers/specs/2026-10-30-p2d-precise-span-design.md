# P2-D 精确诊断 span 设计记录(spec-completion-roadmap 切片;spike 交付物)

> 创建:2026-10-30。上游:`docs/superpowers/plans/2026-09-13-spec-completion-roadmap.md` P2-D
> (§10.2 span 行列/notes/fixes;与 LSP 合并立项建议)。状态对账(2026-10-28):
> 「R 线 r4b 锚(panic 源位置)确认为 parser→AST→interp 全线 span 线程化
> (Rust AST 全节点无 span),独立批次;本切片未动」——本记录立项。

## 1. 现状锚点(实码,非假设)

- **lexer**:Rust 线 Span 已有行列+字节偏移四元组(`token.rs:43`
  `Span{line,col,start,end}`);Token 携带 span ✓——**行源已备,断点在 AST**。
- **check 侧退化**:诊断 Span 恒 `Span::new(1, 1, 0, 0)`(check.rs 91 处)
  ——检查面拿不到节点位置;诊断只有码+消息,行号恒 1。
- **Rust AST**:全节点无 span 字段(`ast.rs` Expr/Stmt 枚举零位置);
  线程化 = 每节点加槽,构造点(parser.rs)逐处传。
- **自举线**:span = 「行号尾槽」——AST 节点尾元素 = 行号字符串
  (sem_type.ct `nline(n)` 取 `n[n.len-1]`;driver_emit ct_reg_line 同源);
  列信息全无;`@LINE` 诊断机制(emit 侧行锚)独立存在。
- **r4b 锚**:`tests/roadmap/r4b_panic_location.panic.ct`(PanicMsgRed)
  = panic 消息须含源位置 → interp panic 路径需 span → 依赖本切片。
- **LSP**:`lsp/` 目录在库;hover/跳转 = 位置→节点反查,同样缺 span。

## 2. 决策记录

| # | 题 | 裁决 | 理由 |
|---|---|---|---|
| D1 | span 载荷 | 节点级单 span(主记号起-止);不做全节点覆盖 | 表达式诊断 90% 主记号足够;全覆盖 = AST 体积×2 |
| D2 | Rust 线实现 | `Expr`/`Stmt` 枚举逐变体加 `span: Span` 字段(结构变体),解析器构造点直传 lexer span | 显式字段 > 旁表(旁表 = 生命周期/同步债);`#[derive(Clone)]` 不变 |
| D3 | 自举线实现 | 行号尾槽扩为「行:列」复合尾槽字符串(`"123:7"`),`nline` 拆分保持返回行(兼容全量既有消费) | 尾槽是既有协议;nline 88+ 消费点零改动;列位新增消费方才拆 |
| D4 | 兼容契约 | `@LINE` 机制不动;节点槽位变化对 AST dump(fmt --ast)为**加列不换形**,固定点逐字节更新一次 | W5b roundtrip 现役腿(geom/stdpkg/fx_use_alias)即门 |
| D5 | notes/fixes | 诊断结构 `{code,msg,span,notes[],fixes[]}`;fixes = (span,替换文本) 有序编辑列表;v1 只有机器可证 fix(拼写建议 = 编辑距离≤2 的同名表) | fix 应用器 = 独立切片;先出数据形 |
| D6 | 传播序 | lexer(已有)→ parser 直传 → check 诊断 → interp panic 消息(r4b)→ emit `#line` 直出(C 编译器诊断对齐源) | r4b 翻转 = 本切片验收锚;emit #line 让 cc 错误指回 .ct |
| D7 | 批次切分 | ①Rust AST span 线程化(检查面行列贯通) ②自举尾槽复合化 ③interp panic 源位置(r4b 翻转) ④notes/fixes 数据形 + 拼写建议 ⑤LSP 反查消费 | 每批独立绿;①是②③④⑤的硬前置 |

## 3. 设计要点

### 3.1 Rust 线线程化(批①)

- `ast.rs`:逐变体加 span —— 变体多(~40 Expr/Stmt),**宏收口**:
  `spanned!(parser, Expr::Int { .. })` 构造助手统一捕获 `self.peek().span`。
- `check.rs`:91 处 `Span::new(1,1,0,0)` → 逐诊断点从被检节点取 span
  (err 签名加 span 参数,机械替换 + 逐点核主记号)。
- 诊断渲染:`file:line:col: E2020: ...`(与 seed 宿主对齐格式),快照表
  一次性翻转(check_suite 期望表在册机制)。

### 3.2 自举尾槽复合化(批②)

- 尾槽 `"123"` → `"123:7"`(行:列;`parser.ct` 节点构造点加列)。
- `nline` 拆冒号返行(既有消费零改);新 `ncol(n)` 供 LSP/诊断新消费方。
- AST dump/固定点:`--ast=dump` 尾槽原样透出 → 固定点更新(roundtrip
  断言逐字节,格式 v2 一次性)。

### 3.3 r4b 翻转(批③)

- interp panic 消息:`"<msg> (file:line:col)"` —— interp 执行栈记当前
  语句 span(执行时随 stmt 携带,r4b 语料 = while 体 panic)。
- `roadmap_suite` r4b 行 `PanicMsgRed → 迁主套件`(翻转协议);
  native 差分同门(panic 消息两侧一致)。

### 3.4 fixes 数据形(批④)

```
Diagnostic { code, msg, span, notes: [Span], fixes: [{span, text, label?}] }
```
- v1 唯一 fix 源:E2020/E2010 未知名 → 同名表最近邻(编辑距离 ≤2);
  恶意补全拒绝(仅纯更名,无插入结构)。

## 4. 风险与开放题

- **AST 体积**:span 四元组×全节点 — 自举线为字符串尾槽增量(~4B/节点),
  Rust 线 16B/节点;解析税在 W5b 实测口径下可忽略(1980 行 json.ct 级)。
- **双镜漂移**:批①/批②必须同批落库(两侧节点槽位协议耦合),拆批即漂。
- **开放题**:①LSP 消费面(批⑤)是否随 `lsp/` 泳道合批——倾向独立;
  ②emit `#line` 对 T20 缓存键的影响(cfg 同款入键问题)——批③定谳;
  ③interp span 携带的性能面(eval 热路径每 stmt 记 span)——r4b 批实测。
