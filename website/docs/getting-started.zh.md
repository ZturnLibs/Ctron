# 入门

从安装到第一个可执行,约一刻钟。`ctc` 是工具链唯一的用户入口:`run` / `check` 不依赖任何 C 工具链,只有 `build` 需要本机 C 编译器。

## 安装

**一键(macOS / Linux)**——自动检测平台与架构、下载匹配 tarball、按 `SHA256SUMS` 校验、解压到 `${CTRON_INSTALL_DIR:-$HOME/.ctron}`,结尾打印 PATH 提示:

```bash
curl -fsSL https://github.com/ZturnLibs/Ctron/releases/latest/download/install.sh | sh
export PATH="$HOME/.ctron/ctron/bin:$PATH"   # 按脚本提示写入 shell 配置
```

**手动**——从 [Releases](https://github.com/ZturnLibs/Ctron/releases) 下载 `ctron-<版本>-<平台>.tar.gz`,解压后把其中的 `ctron/bin` 加入 `PATH` 即可(见[下载](download.md)的产物矩阵)。

**源码**——源码 tarball 附带预发射 C(`prebuilt/*.c`),构建只需要 cc:

```bash
tar xzf ctron-<版本>-src.tar.gz && cd ctron-src-<版本>
make && make install PREFIX="$HOME/.ctron"
```

> 首个 Release(v0.0.1)发布前,上面的下载路尚不可用;此阶段请克隆仓库、按 `compiler/BOOTSTRAP.md` 自举构建。装好后用 `ctc --version` 验证:装机形态输出 `ctron <版本> <git-sha>`,仓库 dev 形态输出 `ctron dev`。

## Hello, Ctron

`ctc new` 生成项目骨架:`Ctron.toml`(目前只有 `name` 一个字段)+ `Ctron.ctcl`(能力声明清单)+ `src/main.ct`(hello 程序)。`ctc run` 是纯解释执行——解析、语义检查、直接求值一条龙,零外部依赖,也是最快的上手路径:

```bash
$ ctc new hello && cd hello
ctc: 已生成 hello/(ctc run hello/src/main.ct 试跑)

$ ctc run src/main.ct
hello, ctron
```

## 静态检查

```bash
$ ctc check src/main.ct
check OK decls=1

$ ctc check src/main.ct --format=json
{"diagnostics":[]}
```

检查即止、不改任何文件;`--format=json` 输出结构化诊断,方便编辑器与 CI 消费。

## 构建可执行

```bash
$ ctc build src/main.ct
ctc: 已构建 /path/to/hello/src/main

$ ./src/main
hello, ctron
```

`build` 先发射等价 C(源旁的 `src/main.c`,可读、自包含、只含系统头),再调本机 cc(`-O2 -w -pthread`)出可执行;`CC` 环境变量可换编译器。cc 缺失时 C 照常发射,并一次性给出两条出路:平台化安装指引(macOS `xcode-select --install` / Linux 发行版 gcc / Windows mingw),或拿 `.c` 去任何有 cc 的机器手动编译。

## 项目模式

`ctc build` 不带参数即项目模式:读 `Ctron.toml` 的 `name`,入口固定 `src/main.ct`,产出 `build/<name>`;`c_src/*.c` 若存在则一并链接(FFI 场景):

```bash
$ ctc build
ctc: 已构建 build/hello

$ ./build/hello
hello, ctron
```

## 顺手记住

- 子命令:`run` / `check` / `build` / `test` / `new`;`ctc --help` 总览,`ctc help <cmd>` 子命令详助。
- 退出码:`0` 成功,`1` 程序诊断失败,`2` ctc 环境或用法错误。
- Windows:`run` / `check` / `new` 零前提;`build` 需 mingw-w64(MSYS2 `pacman -S mingw-w64-x86_64-gcc`,或免安装的 w64devkit),产物带 `.exe`。
- `build` 与 Windows 整体处于 β:能力边界见根 `README.md` 与工具链分发设计(`docs/superpowers/specs/`)。

下一步:[示例](examples.md)——三个完整 CLI 工具的注解源码。
