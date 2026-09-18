# Ctron

自举版 Ctron 工具链:解释执行零依赖;发射等价 C,本机 cc 一条命令出可执行。

## 安装与上手(v0.1)

```bash
curl -fsSL <releases>/install.sh | sh      # 或手动下载 tarball 解压
export PATH="$HOME/.ctron/ctron/bin:$PATH"
```

```bash
ctc run hello.ct    # 解释执行(零依赖)
ctc check hello.ct  # 静态检查(--format=json 出 JSON 诊断)
ctc build hello.ct  # 出可执行(需本机 C 编译器)
```

自举编译器见 compiler/README.md,发布工程见
docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md。
