# ctron-lsp —— Ctron 语言服务器(Ctron 语言实现)

> 单文件 Ctron 程序(`src/main.ct`,~1500 行),经种子编译器 `ctronc run` 解释执行,
> 与自举路线一致:现在解释跑,C10 转译后端成熟后原生跑。
> VSCode 客户端见 `../editors/vscode-ctron/`(打包时 `make package` 自动同步本文件副本)。

## 能力(协议方法)

| 方法 | 内容 | 精度 |
|---|---|---|
| publishDiagnostics | 词法级 E1001 族:未终止字符串/插值、禁用标点 `; ::`、`/*`、char 字面量、非法字符 | 打开/按键即更 |
| documentSymbol | 顶层声明(fn/struct/class/enum/trait/impl/const/static/test/use,识别 `pub`) | 精确 |
| hover | 声明签名 + 相邻 `///` 文档;关键字/前奏类型内置文档(§1.3/§3.8.2) | 同文件 |
| completion | 关键字 + 前奏类型 + 本文档声明(前缀粗筛);`.` 后前奏成员集 | 无类型感知 |
| definition | 同文件声明 | 精确 |
| references / documentHighlight | 同文件出现点(词法级:跳过字符串/注释,不做作用域感知) | 精确 |
| rename + prepareRename | 同文件词法级重命名;关键字/非法名拒绝 | 词法级 |
| signatureHelp | 调用点回溯配平 `(` → 声明签名,形参分段偏移 + activeParameter | 同文件 |
| inlayHint | 调用点形参名(kind=Parameter;跳过声明行/成员访问/关键字;>24KB 文档跳过,40 调用点上限) | 同文件 |
| formatting | 缩进规整(4 空格/层;`} ) ] else` 起始行去层),仅偏差行产出 TextEdit | 词法级 |
| foldingRange | 花括号块(栈式配平,深度上限 64) | 精确 |

位置编码:声明 `utf-8`(character = 行内字节偏移)。文本同步:全量(didOpen/didChange)。

## 架构(源码节)

| 节 | 内容 |
|---|---|
| §0 字节与串基础 | or2/or3(**Ctron 无 `\|\|`,逻辑或 = `!(!a && !b)`**)、chr_ascii 查表、starts_lit、parse_dec |
| §1 游标 | `Cur`/`Pos2` 出参类(Ctron 无多返回值;类实例按引用可变) |
| §2 JSON | 字节级解码(段式拼接防 O(n²);`\uXXXX` 透传)+ jobj/jkv 序列化助手(**字面 `{` 必须写 `\{`**) |
| §3 文档库 | 单实例 `Store`,16 命名槽位 + if 链访问器(**数组字面量按值克隆,索引变更会丢失**) |
| §4 行/词定位 | line_span / word_span_pos(全部单趟,禁止逐行回扫全文) |
| §5 词法诊断 | scan_diag(E1001 族)——语义诊断走客户端包装 `ctronc check --format=json`(§10.2 契约) |
| §6 符号提取 | find_decl / scan_symbols / doc_decl_items / collect_docs——**单趟字节扫描**(O(n);逐行 line_span_of 回扫为 O(行×文长),曾致 61KB 文档 256s,重构后 3.4s) |
| §7 静态知识表 | KW_ALL / PT_ALL / MEM_ALL 管道串 + pipe_has(**零动态集合:管道串 + 计数器**是本实现的通用习语) |
| §8 协议助手 | Frame 读写(read_line/read_bytes/flush_out 三个 I/O 内建)、occ_scan 出现点、sig_help、layout_scan(格式化+折叠单遍双模)、inlay_hints |
| §9 请求分发 | handle(msg, st) if 链;main 循环 |

## 解释器(C4)域内约束——修改必读

- **禁用**:`match` / `enum` 构造 / `List` / 元组表达式 / `break` `continue`(预留字:解析器放行,解释器报"未解析名称")/ `||`(语法不存在,`|` 是闭包界定)
- **逻辑或**:`or2/or3` 助手;**or2 无短路**——`or2(i == n, byte_at(text, i) == 10)` 在 i==n 时右侧仍求值 → 越界,右式必须有前置守卫
- **§1.8 消歧陷阱**:`if depth < dstk[top - 1] {`(索引紧跟块花括号)被误判泛型实例化 → 用临时变量承接
- **数组**:`I64[]` 固定字面量;数组元素按值克隆,**经索引的字段变更会丢失** → 需要可变状态一律用单实例类字段
- **字符串拼接**:只有插值(`"{a}{b}"`);字面 `{` 写 `\{`,字面 `}` 原样;插值内不能出现字符串字面量实参(先存局部变量)
- **顶层 const 可用**(`run` 路径已验证);`static let` 在 run_main 不初始化,勿用
- `else` 必须与 `}` 同行;语句以换行终止

## 构建与测试

```sh
cd ../compiler-c && make          # 解释器宿主
# 端到端:python 构造 Content-Length 帧 | ctronc run lsp/src/main.ct,断言响应
# 回归:全部 tests/*.ct + selfhosted/*.ct + 自身灌入,零 panic、退出码 0
cd ../editors/vscode-ctron && make package   # 打包前自动同步本目录到 server/main.ct
```

## 性能特征(解释器域)

| 文档规模 | 体验 |
|---|---|
| ≤ 8KB(典型) | 各特性 <0.3s,流畅 |
| ~24KB | inlay 上限内可用;其余秒级 |
| >24KB | inlay 跳过(护栏);format/fold/symbol 单趟 61KB ≈ 3.4s |

终态:C10 转译后端可用后,LSP 转译为原生二进制(性能与 sem 嵌入同时解决);届时本文件的
解释器约束节整体作废,算法层单趟设计仍然有效。

## 已知限制

- 诊断仅词法级(语义级由客户端 check 包装提供);
- rename/references 词法级,无作用域感知(同名不同作用域符号会一并处理);
- 跨文件能力(索引/跳转/自动 use)待模块系统(C8)与工作区索引;
- 非 ASCII 行内列偏移按字节(已声明 utf-8 编码,旧客户端 utf-16 下有偏差)。
