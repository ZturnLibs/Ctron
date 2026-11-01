# P2-D 批① span 线程化——入口包(量测地形;2026-11-01 专窗开场即用)

> 上游:specs/2026-10-30-p2d-precise-span-design.md 批①(Rust AST span 线程化,
> 检查面行列贯通)。本包 = 量测地形 + 机械规程 + 切片序,下一专窗零预热开工。

## 量测地形(2026-11-01 grep 实测)

- **ast.rs**:Expr 20 变体 / Stmt ~10 变体;`#[derive(Debug, Clone, PartialEq)]`。
- **结构模式匹配面(加字段即破)**:check.rs 46 / trans.rs 45 / interp.rs 25 /
  parser.rs 34 处 `Expr::<V> { ... }` 形(约 92 处,含 `..` 者自动吸收);
  Stmt 族另计(~15)。
- **构造点**:parser.rs `Expr::` 34 处 / Stmt ~15 处(每处补 span 实参)。
- **检查面**:check.rs `Span::new(1, 1, 0, 0)` 91 处(逐点置换为节点 span 查询)。
- **fmt.rs**:0 处(词法重排,不触 AST)✓。

## 机械规程(切片序;每片独立绿)

1. **id + 侧表**:ast.rs Expr/Stmt 每变体加 `id: u32`;`nid()` thread-local
   计数助手;SPANS 侧表(`Vec<Span>`,id → Span;parse 期填充,check 期读)。
   matches 破面 = 上述 ~92 处逐处补 `..`/字段(机械;编译器驱动)。
2. **parser 直传**:构造点以 `self.peek().span` 填 span 侧表(构造序 = id 序)。
3. **check 91 点**:err() 签名加 span 参;逐点以相关节点 id 查侧表置换。
   诊断格式 `file:line:col: E2020: ...`;check_suite 快照翻转(一次性)。
4. **面审计**:fmt_suite(文本等价,AST PartialEq 不入门)/parse_suite
   (固定点逐字节)/infer_suite——预期零扩大;AST PartialEq = span 同 parse
   同值,固定点安全(ParseResult 全等)。

## 双镜纪律

批① = Rust 线专属;自举线 span(行号尾槽复合化) = 批②,与批① 同窗收尾
(双镜同批:两侧节点协议耦合,拆批即漂)。

## 验收

- check 诊断三态格式(file:line:col);check_suite 快照翻转一次性;
- native_suite/suite.py/fmt/parse/infer 基线零扩大;
- 03m/03n parity 钉维持 58/31/0。
