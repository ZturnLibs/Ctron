# Ctron

**Ctron** 是一门自举的系统编程语言:编译器本身用 Ctron 写成(`compiler/`,39 个模块),解释执行零外部依赖;也能把程序发射为**等价的 C 源码**,交给平台 C 编译器一条命令出原生可执行。同一份源码,两条执行路径。

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
```

> Releases 尚未发布——首次发布(v0.0.1)上线前,请按[入门](getting-started.md)的源码路径构建。

## 三条命令

```bash
ctc run main.ct      # 解释执行,不需要任何 C 工具链
ctc check main.ct    # 静态检查,--format=json 出结构化诊断
ctc build main.ct    # 发射 C → 本机 cc → 原生可执行
```

`ctc` 是随工具链分发的薄驱动(POSIX sh 与 PowerShell 双实现,同一命令面),也是你唯一需要记住的入口。工具链版本与语言里程碑解耦,发布线自 v0.0.1 起。
三条路都是即装即用:没有构建步骤,没有依赖,也没有前置配置。

## 为什么值得看

- **真自举**:编译器全部是 Ctron 源码;三级自举固定点已脚本化复现——不同平台发射出的 C **逐字节相同**,并作为发布承诺进 CI。
- **零依赖即用**:`ctc run` / `ctc check` 是纯解释路径,不需要 C 编译器;只有 `ctc build` 借用本机 cc。
- **C 就是后端**:发射产物自包含——`ctron_*` 运行时全部内联,只含系统头。有 cc 的平台就是目标平台:macOS(arm64 / x86_64)、Linux(x86_64 / arm64)、Windows(x86_64,β)。
- **结构化并发与错误链**:任务跑在 `scope` / `spawn` / `join` 之下——块作用域管理,取消沿 scope 传播;失败以 `Result` 错误链上溯,`message` / `cause` / `trace` 层层可见。
- **标准库即源码**:`use std.*` 在编译期读源码、与程序合并为单一 AST——像 Python 的 stdlib 一样可读可改,不存在"预编译 std"这种黑盒。
- **示例即交付物**:`examples/` 的 ctgrep / ctwc / ctwf 是完整 CLI 工具,发布验收流程直接拿它们当考题。

## 导航

| 页面 | 内容 |
|---|---|
| [入门](getting-started.md) | 安装三法、hello ctron、第一个可执行、项目模式 |
| [示例](examples.md) | ctgrep / ctwc / ctwf 注解源码与复现命令 |
| [下载](download.md) | 五平台产物矩阵、校验、Windows 说明 |
| [语言规范](spec/README.md) | v0.7 冻结草案,诊断一律带稳定错误码 |
| [标准库参考](std/README.md) | 模块签名表 |

仓库:[ZturnLibs/Ctron](https://github.com/ZturnLibs/Ctron) · 设计与实现细节见 `docs/superpowers/specs/` 与 `compiler/BOOTSTRAP.md`。
