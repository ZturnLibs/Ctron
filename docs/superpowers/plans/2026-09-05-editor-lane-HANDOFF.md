# Ctron 编辑器泳道 交接文档(新 session 从这里开始)

> 创建:2026-09-05(编辑器支持 session)。与 `2026-09-05-lane-split.md`(泳道协议)、
> `2026-09-05-c-bootstrap-HANDOFF.md`(自举泳道)并列。新编辑器 session 请先读本文件、
> `docs/editor-vscode-support.md`(总体分析)、`docs/editor-feature-gap-analysis.md`(差距矩阵)。

## 0. 交付物总览(v0.3.0,全部已提交)

| 路径 | 内容 |
|---|---|
| `editors/vscode-ctron/` | VSCode 扩展:TextMate 高亮、language-configuration(§1.6 延续集缩进)、22 片段、**手写零依赖 LSP 客户端**、check 包装(保存时 `ctronc check --format=json` 语义诊断 + fixes[] CodeAction + 词法确定性修复)、`make package` 打包流水线 |
| `lsp/src/main.ct` | **LSP 服务器,用 Ctron 语言实现**(单文件 ~1500 行,`ctronc run` 解释执行);架构/约束/性能见 `lsp/README.md` |
| `compiler_c/src/rt.c` | 跨泳道最小改动(按泳道协议):I/O 内建 `read_line`/`read_bytes`/`flush_out`;`call_decl` 实参求值语义修复(21a5b3e);byte_at 快路径因 UB 被自举泳道回退(正确取舍) |
| `docs/` | 总体分析、差距矩阵(对照 rust-analyzer/gopls/zls)、本交接 |

已交付特性:高亮(插值/后缀/非法标点标红)、缩进规则、片段、词法诊断、语义诊断(保存时)、
大纲、hover(签名+`///`+关键字/前奏文档)、补全、定义跳转、同文件 rename/references/
documentHighlight、signatureHelp、形参名 inlay hints、缩进格式化、花括号折叠。

## 1. 命令速查

```sh
cd compiler_c && make && make test              # 宿主 + 全套回归
cd editors/vscode-ctron && make package         # 同步 lsp 源 → server/main.ct 副本 → vsce 打包
code --install-extension ctron-lang-0.3.0.vsix  # 本地安装
# 服务器端到端调试:python 构造 Content-Length 帧 | ctronc run lsp/src/main.ct
```

验收口径:① `ctronc parse lsp/src/main.ct` 零自诊断;② 全部 tests/*.ct + selfhosted/*.ct +
自身灌入服务器(诊断/inlay/format/fold/symbol 全特性)零 panic、退出码 0;③ P1/P2 断言脚本
(响应 JSON 逐字段)通过。

## 2. 解释器域编程钉子(写 Ctron 必读——A 泳道同款坑)

以下全部为本 session 实测踩坑,写任何解释器域 Ctron 程序都适用:

1. **无 `||`**:语法不存在(`|` 是闭包界定,解析器报构造字面量类错误)。逻辑或 = `or2/or3`
   (`!(!a && !b)` 形)。**or2 无短路**:`or2(i == n, byte_at(text, i) == 10)` 在 i==n 时右侧
   仍求值 → 越界 panic。守卫必须显式前置。
2. **`break`/`continue` 是预留字**:解析器放行,解释器运行时报"未解析名称"。循环退出一律
   `var done = false; while !done { … }` 标志模式。
3. **§1.8 消歧陷阱**:`if d < arr[top - 1] {`(索引表达式紧跟块花括号)被解析器误判为
   泛型实例化(规则:`IDENT[ … ]` 紧跟 `(` 或 `{` → 实例化)。用临时变量承接下标值绕开;
   解析器回退逻辑待 B 泳道改进。
4. **数组字面量按值克隆**:`docs[1].text = "x"` 经索引的字段变更会丢失。可变状态一律用
   **单实例类字段 + 命名槽位 + if 链访问器**(见 Store)。顶层 `const` 在 `run` 路径可用;
   `static let` 不初始化,勿用。
5. **字符串**:拼接只有插值累加(段式 `byte_slice` 可防 O(n²));字面 `{` 写 `\{`,字面
   `}` 原样;插值内不能出现字符串字面量实参(先存局部变量)。
6. **动态集合不存在**(List/enum/match/元组在解释器域不可用):固定数组字面量 + 计数器、
   管道串(`"a|b|c"` + pipe_has/pipe_nth)、字符串累积是标准习语。
7. **多返回值不存在**:出参用类实例(`Cur`/`Pos2`);类实例按引用可变,数组内取出的实例是
   副本。
8. **性能**:byte_at/每次求值 ~µs 级;**任何逐行调 line_span_of 回扫全文的写法都是
   O(行×文长)**(61KB 曾 256s)。一律单趟字节扫描(61KB → 3.4s)。

## 3. 已知限制与下一步

限制(均已在 README 声明):rename/references 词法级无作用域感知;语义诊断依赖保存;
非 ASCII 行内列偏移按字节;>24KB 跳过 inlay hints。

下一步(按差距矩阵 §5 优先级):
1. **semantic tokens**(轻命名解析:声明/调用/成员/内建分类着色;服务器 + legend 接线,~3 天);
2. **真解析器接入**:合并 selfhosted `parsetree.ct`(159 用例差分锁定)进 LSP → 精确语法
   诊断区间/selectionRange;注意单文件解释器约束(合并或等 C10 多模块);
3. **测试集成**:`ctronc test` CLI(需 `ctron_rt_run` 暴露 filter;跨泳道,先与 A 泳道协调)
   → 逐测试 CodeLens / Test Explorer;
4. canonical **ctron-fmt**(规范唯一形态)→ 替换当前缩进级格式化;
5. 跨文件:工作区索引 → workspace symbol / 跨文件 def/refs / 自动 use(待 C8 模块系统)。

## 4. 协作记录(泳道协议执行情况)

- rt.c 属 A 泳道;两次跨泳道改动均按协议(最小 diff + make test 绿 + 动机说明)执行;
- **教训**:call_decl 修复曾被并行提交的编辑器缓冲覆盖(未提交窗口事故),21a5b3e 重放。
  跨泳道改动请**改完立即提交**,勿留窗口;
- `docs/editor-feature-gap-analysis.md` 的 P1/P2 状态行由本泳道维护。
