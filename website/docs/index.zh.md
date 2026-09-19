# Ctron：AI Native 系统编程语言

Ctron 从设计之初就以 AI 生成代码为主要使用方式。语法、类型系统、错误模型、诊断格式，每一层都围绕"AI 写、人审"这个前提做出取舍。

## 设计原则

**AI 写，人类读。** AI 生成的代码结构正确但细节出错——变量名拼错、类型不匹配、遗漏边界条件。Ctron 把这类错误挡在编译期：每个错误带稳定错误码，诊断结构化输出，类型推断有明确规则。AI 收到错误码后可以精确修正，不需要从自然语言描述中猜意图。

**语义清晰，所见即所意。** 一个赋值语句不会触发构造函数或自定义运算符。没有 null，没有隐式数值转换，没有宏，没有未定义行为。代码做了什么，就是它看起来的样子。

**没有魔法。** 标准库是 Ctron 源码，不是编译好的二进制。编译器也是 Ctron 写的。AI 可以读标准库源码理解 API 行为，不需要查文档。

**在清晰无歧义的前提下尽可能简洁。** `?` 做错误传播，`scope` 做结构化并发，`use` 做模块导入。每个语法构造只做一件事。

## 三条命令

```bash
ctc run main.ct      # 解释执行，不需要任何 C 工具链
ctc check main.ct    # 静态检查，--format=json 出结构化诊断
ctc build main.ct    # 发射 C → 本机 cc → 原生可执行
```

解释路径零外部依赖。发射路径自包含——运行时全部内联，只含系统头文件。同一份源码，开发和部署不换语言。

## 真自举

编译器全部是 Ctron 源码。三级自举固定点已脚本化复现：不同平台发射出的 C 逐字节相同，并作为发布承诺进 CI。

自举证明语言能写真实软件。标准库也是 Ctron 源码——AI 可以读标准库源码理解 API，不需要查文档。

## 结构化并发与错误链

任务跑在 `scope` / `spawn` / `join` 之下——块作用域管理，取消沿 scope 传播。失败以 `Result` 错误链上溯，`message` / `cause` / `trace` 层层可见。`Send` 检查在编译期完成。

## 导航

| 页面 | 内容 |
|---|---|
| [入门](getting-started.md) | 安装三法、hello ctron、第一个可执行、项目模式 |
| [示例](examples.md) | ctgrep / ctwc / ctwf 注解源码与复现命令 |
| [下载](download.md) | 五平台产物矩阵、校验、Windows 说明 |
| [语言规范](spec/README.md) | v0.7 冻结草案，诊断一律带稳定错误码 |
| [标准库参考](std/README.md) | 模块签名表 |

仓库:[ZturnLibs/Ctron](https://github.com/ZturnLibs/Ctron) · 设计与实现细节见 `docs/superpowers/specs/` 与 `compiler/BOOTSTRAP.md`。
