<div align="center">

[English](README.md) | 简体中文 | [Deutsch](README.de.md) | [Français](README.fr.md) | [Español](README.es.md) | [日本語](README.ja.md)

</div>

# Ctron

**AI Native 系统编程语言** —— 为「AI 写、人审」而生:错误挡在编译期,语义所见即所意,没有魔法。

```bash
ctron run main.ct      # 解释执行,零外部依赖
ctron check main.ct    # 静态检查,--format=json 出结构化诊断
ctron build main.ct    # 发射等价 C → 本机 cc → 原生可执行
```

- 仓库:<https://github.com/ZturnLibs/Ctron>
- 文档站:<https://zturnlibs.github.io/Ctron/>(中英双语)

---

> **项目状态:** Ctron 处于开发期,**不建议用于生产环境**——语言与工具链仍在快速演进。欢迎有兴趣的开发者一起共建:见[参与共建](#参与共建)。

## 为什么是 Ctron

AI 生成的代码有一个稳定特征:**结构正确,细节出错**。变量名拼错、类型不匹配、遗漏边界条件——这些错误在动态语言里要到运行时才暴露,在 C/C++ 里可能直接变成未定义行为。Ctron 的应对不是"更好的 AI",而是更严格的语言设计:

- **每个诊断带稳定错误码**(E2010、E2030、E3040…)。AI 收到错误码即可精确修正,不需要从自然语言描述里猜意图。
- **诊断结构化输出**(`ctron check --format=json`),编辑器与 CI 管道直接消费。
- **语法无歧义、类型推断规则明确**——同样的代码,任何地方给出同样的裁决。

**没有魔法。** AI 写代码最怕的不是语法复杂,而是语言在暗处做了什么:

| Ctron 不做的事 | 替代方案 |
|---|---|
| 无 null | `Option[T]` |
| 无隐式数值转换 | 显式 `as[T]()` |
| 无运算符重载 | `+` 就是加法 |
| 无宏 | 没有文本替换,源码即真相 |
| 无未定义行为 | 规范逐条列出合法行为,没写的就不存在 |

**一切皆源码。** 标准库是 Ctron 源码,不是编译好的二进制——`use std.*` 在编译期读源码、与程序合并为单一 AST。AI 可以直接读标准库源码理解 API 行为,不查文档。

## 快速上手

```bash
# macOS / Linux 一键安装(自动校验 SHA256)
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/bin:$PATH"
```

```bash
$ ctron new hello && cd hello     # 生成项目骨架:Ctron.toml + Ctron.ctcl + src/main.ct

$ ctron run src/main.ct           # 解释执行:解析、语义检查、求值一条龙
hello, ctron

$ ctron check src/main.ct --format=json
{"diagnostics":[]}

$ ctron build src/main.ct         # 发射可读、自包含的 C,再调本机 cc
$ ./src/main
hello, ctron
```

`run` / `check` / `new` 不依赖任何 C 工具链;只有 `build` 需要本机 cc(`CC` 可换编译器)。Windows 下 `run` / `check` 零前提,`build` 需 mingw-w64。退出码约定:`0` 成功 / `1` 程序诊断失败 / `2` 环境或用法错误。

## 语言一瞥

无 null、`Result` 传播、错误链、语言内测试:

```ctron
@derive(Error)
enum MathErr {
    DivByZero
}

fn safe_div(a: I32, b: I32) -> Result[I32, MathErr] {
    if b == 0 { return Err(DivByZero) }
    return Ok(a / b)
}

fn ratio(a: I32, b: I32, c: I32) -> Result[I32, MathErr] {
    let x = safe_div(a, b)?                   // `?` 传播,自动附加上下文
    return safe_div(x, c)
}

test "result propagation with ?" {
    assert_eq(ratio(100, 5, 4).or(-1), 5)
    match ratio(1, 0, 2) {
        Err(DivByZero) => assert(true)
        Ok(_)          => assert(false)
    }
}

test "error context chain" {
    let res = ratio(1, 0, 1).context("computing ratio")
    match res {
        Err(e) => {
            assert_eq(e.message, "computing ratio")
            assert(e.cause.is_some())          // cause 链保留根因
        }
        Ok(_) => assert(false)
    }
}
```

一个真实的小工具(`examples/ctwc`,wc 式统计):

```ctron
use std.str.{words, count_ch}

fn main() -> I32 {
    var path = ctron_entry()
    if path == "" {
        println("ctwc: usage: ctwc run <file>")
        return 1
    }
    match read_file(path) {
        Some(s) => {
            var nw = words(s).len
            var nl = count_ch(s, 10)
            println(nl.to_string() + " " + nw.to_string() + " " + s.len.to_string() + " " + path)
        }
        None => {
            println("ctwc: cannot open " + path)
            return 1
        }
    }
    return 0
}
```

再往上的能力:

- **结构化并发**——任务跑在 `scope` / `spawn` / `join` 之下,生命周期绑定块作用域,取消沿 scope 传播,任务 panic 触发结构化取消广播;`Send` 检查全部在编译期完成。
- **内存两档**——GC 之上提供 `own` 块(作用域内无 GC 内存子集)与 `Arena`;`bare` 档完全拒绝 GC 分配,可面向裸机单片机。
- **编译期执行**——`const` / `comptime fn` 编译期求值;`@derive(Show, Eq)` 自动合成常用 impl;`///` 文档注释中的代码块会被编译并执行(doc-test)。
- **能力显式注入**——I/O 权限是显式的 `Cap` 对象,在包清单中声明;越权编译期即拦截。
- **FFI 一等公民**——`extern "c"` 声明 + `c_src/*.c` 自动一并链接。

## 两条执行路径

```
ctron run main.ct      解释执行:零外部依赖,开发迭代最快路径
ctron build main.ct    发射 C → 本机 cc:原生可执行,部署不换语言
```

发射产物是**可读、自包含的 C**(运行时全部内联,只含系统头),留在源码旁可审可查。同一份源码贯穿开发与部署。

## 标准库与域包

标准库以**源码形态**随工具链分发,域包挂顶层命名空间:`use std.*` / `use net.*` / `use gui.*`。

| 包 | 内容 |
|---|---|
| `std` | 26 个模块:json / csv / uuid / crypto(SHA-256、HMAC、PBKDF2)/ fmap / heap / iter / sort / unicode / strconv / rand / path / time… |
| `net` | TCP 传输门面:listen / connect / accept / read / write |
| `http` | HTTP/1.1 解析器 + 客户端 + WebSocket + SSE + **Web 框架中间件族**(router / JWT auth / cors / csrf / openapi / metrics / timeout / static…) |
| `tls` | TLS 门面 |
| `db` | 纯 Ctron 线协议驱动:**PostgreSQL** / RESP2(Redis)+ 连接池 + 行映射 |
| `ffi` | C 边界错误包装(errno 折叠进 `Result`) |

## 工具链一览

`ctron` 是唯一用户入口:

| 命令 | 作用 |
|---|---|
| `run` / `check` / `build` | 解释执行 / 静态检查(JSON 诊断)/ 发射构建 |
| `test` | 运行语言内 `test` 块 |
| `fmt` | 规范格式化 |
| `lint` | 诊断汇总(`--strict` 把警告升级为错误) |
| `doc` | 接口投影:公开符号表 + 契约注释 |
| `bench` | 基准族:lang / gc / http / net / ffi |
| `new` | 项目脚手架 |
| `add` / `publish` / `lock` | 装包 / 发包 / 锁文件 |

## 编辑器支持

- **语言服务器**(`lsp/`)——诊断、documentSymbol、hover、补全、定义/引用、rename、signature help、inlay hint、格式化、折叠。
- **VSCode 扩展**(`editors/vscode-ctron`)——语法高亮、代码片段、保存时语义诊断与快速修复、语义 token、参数名 inlay hint;附带 CTML(GUI 标记)与 CTCL(配置语言)支持。

## 当前状态与边界

- 首个 release **v0.0.1** 已出;语言规范为冻结草案(v0.8)。若下载路不可用,可从源码构建——见 [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md)。
- 适用:CLI 工具、网络服务、编译器、解析器。
- 不适合:GPU 密集计算;硬实时系统(GC 停顿不可控——`bare` 档缓解但未消除)。
- `build` 与 Windows 整体处于 β。

## 参与共建

Issue、示例、文档、代码都欢迎——多数贡献不需要了解编译器内部。

**报缺陷**——开 issue 附最小 `.ct` 复现件与 `ctron check <file> --format=json` 输出。诊断自带稳定错误码,修复可以很精确。

**做生态**——标准库模块、示例、编辑器工具、文档与网站:直接提 PR。

**演进语言**——规范为冻结草案:先在 issue 里做设计讨论;被接受的变更以「规范修订条目 + `tests/` 语料钉子」落地。语言行为必须三线对齐(C 种子 / Rust 参考 / 自举编译器),以一致性测试集为仲裁。新诊断必须在规范的错误码注册表登记稳定错误码。

**开发流程**

```bash
git clone https://github.com/ZturnLibs/Ctron && cd Ctron
bash ci.sh    # 一条命令:引导自举工具链,跑满 9 级验收门禁
```

PR 以 `ci.sh` 全绿为准。`ctron`(sh)与 `ctron.ps1` / `ctron.cmd` 保持行为同文。提交信息遵循 conventional commits(`feat:` / `fix:` / `docs:`)。

## 文档导航

| 文档 | 内容 |
|---|---|
| [docs/spec/](docs/spec/README.md) | 语言规范 v0.8(12 章) |
| [compiler/BOOTSTRAP.md](compiler/BOOTSTRAP.md) | 从源码构建工具链 |
| [examples/](examples/) | 完整示例应用 |
| [website/](https://zturnlibs.github.io/Ctron/) | 入门、标准库参考、示例、下载 |

## 许可证

MIT——详见 [LICENSE](LICENSE);[中文参考译文](LICENSE.zh-CN.md)仅供参考,以英文原版为准。
