# pkg 包 —— ctron pkg 库半面(registry 包;P8-D)

registry v1 + CTCL 子集解析的**库半面**;CLI 落点 = 仓库 `tools/pkg.ct`
独立程序(零编译器树扰动,拍板 2026-09-26)。

参考页:[pkg.md](pkg.md)(签名面自动生成)。

## 协议 v1(纯 GET 静态约定)

- `/<name>/<ver>/manifest` —— CTCL 文本
- `/<name>/<ver>/module` —— 单文件源码(多文件 tar 志向)

lockfile v1 = `ctron-lock.ndjson` 管道行(`name|ver|hash16` 每包一行;
NDJSON 对象形随字符串转义基建)。

相关:lockfile/workspace/add/publish 已落;闭源包分发的信任协议与
发布契约由 `ctron pkg verify`/`ctron publish` 工具面承载。
