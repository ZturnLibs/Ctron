# Ctron C 版:C5 自举路线(优先目标)

> 战略转向:自举优先。参照 §14.1 阶梯:ctron0-C 逐步接受"用 Ctron 写的编译器模块",逐字差分(61 文件金丝雀),最终 ctronN 编译自己。
> 判定口径:自举"开始" = **第一个 Ctron 写的编译器模块能在本工具链运行且输出与 C 版逐字一致**。

## C5a 自举入口(✅ 本文件已交付)

- `ctronc run <file>`:执行 Ctron `fn main`,输出进 stdout,退出码 = main 返回值。
- Ctron 侧运行内置(与语料测试解释器同构):`read_file(Str) -> Str?`(Option)、`print/println(Str)`、整数/布尔 `.to_string()`、`+` 字符串拼接。
- `selfhost/hello.ct`:`fn main` 读 `tests/01_basics.ct`、`.len`/`.to_string()`/拼接/match/print 全链路。
- `suite_run`:运行 `selfhost/*.ct` 的 main,核对退出码与输出;八件套 + ASan 全绿。

## 自举阶梯(C5b 起,建议顺序)

1. **C5b 字符串/集合面补齐**:字节迭代与下标、逐字符 UTF-8 前进、append 构建、关键字/序号映射——Ctron 写词法器的最小依赖面。
2. **C5c 第一个 Ctron 词法器**:在 `selfhost/lexer.ct` 用 Ctron 实现(标识符/关键字/数字/运算符/注释/换行;字符串与插值后续分批),`fn main` 读语料文件、输出规范 token 文本。
3. **差分 harness**:Ctron lexer 输出 vs C 版 `lex` 的 token 流 **逐字比对**(61 文件金丝雀的第一层),先 1 文件再全量——此即 §14.1 流式移植的第一个闭环。
4. 之后:解析器、类型检查等模块照此逐个移植+差分+删除 C 版;完整固定点(ctronN 自编译逐位一致)为远期终点。

## 冻结纪律

自举代码会踩中语义含混(如 `08_bare.ct` 频次和 6 vs 4);每处先裁定再让 Ctron 代码使用,避免"自举期改语言"。
