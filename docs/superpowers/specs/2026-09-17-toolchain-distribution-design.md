# 自举版 Ctron 工具链分发设计(五平台 v0.1.0)

> 日期:2026-09-17
> 状态:已实施(计划1 4f8f1d1..ff52ec3 + 计划2 至 861689d)
> 修订:2026-09-19 计划2 Task6 按实现同步——§2.1 VERSION 两字段、§2.3 tarball 无 bootstrap、
> §4 test/项目模式/--version 实际口径、§5.8 钉法措辞、§9 补发布首跑风险
> 范围:自举编译器(`compiler/`,现役自举版本)对外发布的打包、安装、运行与 CI 工程化。
> 不含:包管理器/第三方依赖解析、LSP/编辑器插件分发(另行立项)。

## 0. 背景与现状

自举链(`compiler/BOOTSTRAP.md`)产出两个原生二进制;本设计新增第三个:

| 二进制 | 来源 | 能力 |
|---|---|---|
| `ctron-cc` | `build/cc_run.ct` | parse → 语义 12 项 → 解释执行(`<bin> run <file>`) |
| `ctron-emit` | `build/cc_emit.ct` | parse → 发射等价 C(`<bin> run <file> > out.c`) |
| `ctron-chk` | `build/cc_check.ct`(**新增原生化**,现仅 seed 解释形态) | parse → 语义检查即止(`check OK decls=N`,JSON 诊断) |

支撑分发决策的三个已验证事实:

1. **发射 C 完全自包含**:`ctron_*` 运行时全部内联进产物,只含系统头
   (stdio/stdint/string/stdlib/dirent/pthread/setjmp/time)——没有"运行时库"要分发,
   用户产物只额外依赖本机 C 编译器。
2. **发射固定点逐字节**:三级自举固定点(G1/G2 发射产物逐字节相同)已脚本化复现,
   可升级为发布承诺(CI 双平台发射 diff 验证)。
3. **read_file 锚不碍事**:发射产物中只有字面量 `CTRONCLIINPUT` 锚调用被覆靶到 CLI
   输入(`trans_expr.ct:784`),模块加载器的动态路径 `read_file` 是真实文件读取——
   装机后的 std 加载机制无需改动,只需改**路径解析策略**。

现状缺口:std 按种子约定从入口包旁 `../std/` 解析(`parse_pkg.ct:171`),装机后不成立;
二进制无 `--version`;无统一用户 CLI;无发布工程。

## 1. 决策记录

| 决策 | 结论 | 理由与落选项 |
|---|---|---|
| 发布形态 | **预编译 + 源码双线** | 源码线附预发射 C(免 C 宿主 seed,一条 cc 命令自建),保住可审计性;预编译线把上手成本压到一条 curl。落选:纯源码发布(门槛高)、仅 CI 产物(不解决问题) |
| 预编译平台 | **darwin arm64/x86_64 + linux x86_64/aarch64 + windows x86_64(β)** | 发射后端是 C,可移植性天然好;Windows 见 §8 mingw 路线 |
| 用户 CLI | **薄驱动脚本 `ctc`**:POSIX sh 版(mac/linux)+ PowerShell 版 + cmd 垫片(Windows) | 零编译器面新增风险;Ctron 自写驱动需运行时新增子进程能力(现无),挂账 v0 后 |
| Windows 工具链 | **mingw-w64**,MSVC 不入 v0 | 发射 C 含 `pthread.h`/`dirent.h`,MSVC 两样皆无;winpthreads/dirent 在 mingw-w64 齐备。MSVC = 并发运行时重写,挂账 |
| 版本号 | `v0.1.0` 起,工具链版本与语言 milestone 解耦 | 避免语义负担 |

## 2. 发布产物与包布局

每次发布(tag 触发)出 **6 个产物 + 1 个校验单**:
预编译五件(4 × tar.gz + 1 × zip)+ 源码一件(tar.gz)+ `SHA256SUMS`。

### 2.1 预编译线(mac/linux,`ctron-vX.Y.Z-<os>-<arch>.tar.gz`)

```
ctron/
├── bin/
│   ├── ctc            # sh 驱动,用户唯一入口(§4)
│   ├── ctron-cc
│   ├── ctron-chk
│   └── ctron-emit
├── lib/ctron/std/     # 标准库全部 .ct 源码(见下方"为何是源码")
├── share/doc/         # README、语言规范速览、BOOTSTRAP 摘要、examples/
└── VERSION            # "0.1.0 <git-sha>"(两字段,release.sh 生成;ctc --version 直出)
```

**为何 std 以源码分发(结构性必要,非文档性附带)**:`use std.X` 的消费方式是
编译期读源码 → 解析 → 与用户程序**合并成单一 AST**(`pkg_load_use`,单文件程序模型),
解释路径解释该 AST,发射路径把 std 以 C 形态内联进产物——不存在"预编译 std"这种
产物形态;消灭源码分发需先发明 AST/字节码缓存格式(语言级工程,后期规划见 §10.1)。
业界常态同此:Python/Node/Ruby/Go 的 stdlib 均以可读源码躺在用户机器上,反例
(Rust rlib / Java jimage)都是有稳定预编译产物格式的编译模型。成本:208KB/1.2 万行,
每次编译重复解析在 v0 规模可忽略;安装时预编译缓存见 §10.1。

**符号碰撞语义**(分发场景已封死最危险路径):use 为**显式符号导入**(pub 门 +
`E2020`),与入口/已合并模块同名即 **`E5030` 硬拦截**(不再"首个胜出"静默遮蔽)——
用户 `fn parse` 撞 std.json 的 `parse` 是编译错误,不是静默换语义。残余缺口与
远期方向见 §5 第 13/14 项与 §10.2。

### 2.2 预编译线(Windows,`ctron-vX.Y.Z-windows-x86_64.zip`)

```
ctron\
├── bin\
│   ├── ctc.cmd        # cmd 垫片 → powershell -NoProfile -ExecutionPolicy Bypass -File ctc.ps1
│   ├── ctc.ps1        # PowerShell 驱动,与 sh 版同一命令面
│   ├── ctron-cc.exe
│   ├── ctron-chk.exe
│   └── ctron-emit.exe
├── lib\ctron\std\
├── share\doc\
└── VERSION
```

标记 **beta**。前提:用户侧装 mingw-w64(MSYS2 `pacman -S mingw-w64-x86_64-gcc`,
或单包免安装的 w64devkit),仅 `ctc build` 需要;`ctc run`/`check` 零前提。

### 2.3 源码线(`ctron-vX.Y.Z-src.tar.gz`)

```
ctron-src/
├── compiler/src/*.ct      # 36 模块编译器源
├── std/*.ct
├── prebuilt/
│   ├── ctron-cc.c         # 预发射 C ×3(自举固定点保证与任何平台发射产物逐字节一致)
│   ├── ctron-chk.c
│   └── ctron-emit.c
├── ctc / ctc.ps1 / ctc.cmd
├── Makefile               # make: cc 三件 → bin/;make install PREFIX=…(cc-only;无 bootstrap 目标)
├── install.sh             # (+ install.ps1 可选)
└── VERSION
```

**`make` 只需要 cc**——直接编译 `prebuilt/*.c` 即得完整工具链,不经过 C 宿主 seed。
tarball 无 bootstrap 目标(源码线 cc-only);seed 引导(从 .ct 源走 seed 重建发射器)
走 git 仓库,属贡献者场景,不进 tarball。stage0 发布法:seed 只活在信任链起点。

## 3. 安装方式

| 路 | mac/linux | Windows |
|---|---|---|
| 一键 | `curl -fsSL <releases>/install.sh \| sh`:uname 检测 → 下载 tar.gz → sha256 校验 → 解压到 `${CTRON_INSTALL_DIR:-$HOME/.ctron}` → 打印 PATH 提示。顺带检测本机 cc,缺失则给平台化指引(`xcode-select --install` / 发行版 gcc / mingw)。依赖:POSIX sh + curl + tar,零 root | —(`install.ps1` 挂账 §10) |
| 手动 | 解压 + `export PATH=<目录>/ctron/bin:$PATH` | 解压到 `%LOCALAPPDATA%\ctron` + `setx PATH` 或系统设置 |
| 源码 | `make && make install PREFIX=…` | MSYS2 shell 里 `make && make install` |

Homebrew formula 设计留位(布局即 brew prefix 习惯),v0 不做。
macOS Gatekeeper:ad-hoc 签名 + 文档 `xattr -cr` 指引;Developer ID 公证挂账。

## 4. 运行方式(ctc CLI 面)

两版驱动(sh / PowerShell)**同一命令面**,由 conformance 小用例锁定参数兼容:

```bash
ctc run main.ct      # 解释执行,不需要 cc —— 最快上手路径
ctc check main.ct    # 静态检查(文本 / --format=json 诊断)
ctc build main.ct    # ctron-emit 发射 C → 本机 cc → 出可执行(FFI 项目按 Ctron.toml 链接 c_src)
ctc build            # 项目模式:读 Ctron.toml(name/c_src)+ Ctron.ctcl(caps,见下)
ctc test             # test 块执行:无 main 走解释;含 main 文件的 test 块挂账(见下)
ctc new myapp        # 脚手架:src/main.ct + Ctron.toml + Ctron.ctcl(两件齐写)
ctc --version        # ctron 0.1.0 <git-sha>(VERSION 两字段直出)
ctc --help           # 总用法(= ctc help / -h);ctc help <cmd> 看子命令详助
```

环境变量:`CC`(mac/linux 默认 `cc`;Windows 默认 `gcc`)、`CTRON_STDPATH`(覆盖标准库位置)。

**项目模式 caps 口径(按实现)**:项目 = **双文件**——`Ctron.toml`(name、c_src 链接,
ctc 层消费)+ **`Ctron.ctcl`**(CTCL 清单,`caps` 声明由编译器 `pkg_caps_allowed` 消费,
与编译器同一解析源,越权即 `E4010` 拦截);`ctc new` 两件齐写。

**`ctc test` 口径(按实现)**:无 main 文件的 test 块由解释器直接执行;**含 main 文件的
test 块执行挂账**——解释器(含 main 即走 main)与发射面均无此路,宿主(仓库自测)以
check/test 两阶段兜底(BOOTSTRAP.md 口径),用户面 `ctc test` 暂同解释口径。

**帮助与用法面**(sh/PS 双驱动同文,进 conformance 用例):

- `ctc --help` / `-h` / `ctc help` → 总用法:子命令一行一条、环境变量、rc 约定摘要,rc=0;
- `ctc help <cmd>` 与 `ctc <cmd> --help` → 子命令详助(参数、产物位置、示例),rc=0;
- `ctc` 裸调与未知子命令 → 用法摘要 + "详见 ctc --help",rc=2(用法错误属 ctc 自身
  前置面,与 rc 约定一致);
- 帮助文本两版内容一致(conformance 归一化空白后比对);帮助由 ctc 层负责,
  三个 `ctron-*` 二进制的裸 `run <file>` 契约不变、不加帮助分支。

平台差异(全部收在驱动层):

- cc 统一参数 `-O2 -w -pthread`(glibc < 2.34 需要 -pthread;clang 接受无害);
- Windows 链接加 `-Wl,--stack,<bytes>`:**编译器三件 16MB**、`ctc build` 用户产物默认
  8MB(`--stack=N` 可覆盖)。依据:Windows 主线程默认栈 1MB(mac/linux 8MB),ceval 已知
  ~1800 步深递归击穿 8MB 宿主栈(BOOTSTRAP.md §7)——1MB 下必炸,不是可选项;
- Windows `build` 产物带 `.exe` 后缀。

文档口径:`run`/`check` 成熟路径;`build` 标 **beta**(发射器能力面仍有 README 记载的
挂账,部分形态命中即 panic);Windows 整体标 beta。

**`ctc build` 的 cc 缺失路径**(环境前置失败,与程序诊断分离):

1. 预检 = cc 编译冒烟,不是存在性检查:`echo 'int main(){return 0;}' | $CC -x c - -o <tmp>`——
   只查 `command -v` 不够(macOS 无 CLT 时 `/usr/bin/cc` 是存在但必报错的垫片;PATH 坏、
   头文件缺同理);
2. 预检失败**仍发射 C**(发射 <0.1s 且本身是合法产物),错误信息一次性给出两条出路:
   ① 平台化安装指引(mac `xcode-select --install` / linux 发行版 gcc / windows MSYS2
   或 w64devkit);② 已生成的 `.c` 位置(单文件模式:`<stem>.c` 与源同目录;项目模式:
   `build/`)——可手动编译,或拿到任何有 cc 的机器上编译;
3. rc 约定:`0` 成功 / `1` 程序诊断失败(含负例拦截)/ `2` ctc 自身环境前置缺失
   (cc 不可用、std 找不到等)——沿用 ctc.sh 现有"缺宿主 seed rc=2"惯例;
4. `run`/`check`/`test`/`new` 均不经过 cc,此故障隔离在 build 路径。

## 5. 仓库改造清单

| # | 改造 | 位置 | 平台 | 说明 |
|---|---|---|---|---|
| 1 | std 解析三级查找 | `parse_pkg.ct` | 全 | ① `CTRON_STDPATH` ② exe 旁 `../lib/ctron/std` ③ 回落 `../std`(种子约定)。③ 保证仓库内现有测试与自举链**零破坏** |
| 2 | `ctron_exe_path` 内建 | `driver_emit.ct` 样板 + `rt_core.c` | 全 | 发射样板 `#ifdef` 三分支:linux `readlink("/proc/self/exe")`、darwin `_NSGetExecutablePath`、windows `GetModuleFileNameA`;seed rt 加同名内建**恒空串**(镜像 `ctron_entry` 口径)→ seed 下走回落 ③。Win32 API 返回反斜杠路径,Win32 文件 API 与 mingw CRT 接受混用分隔符,路径拼接零改动(实测有问题再归一化) |
| 3 | `ctron-chk` 原生化 | `native.sh` | 全 | 第三个二进制,照抄现有两件模式 |
| 4 | 版本锚注入 | `build.sh` + 三驱动 | 全 | `ANCHORVERSION` 锚拼接时替换(镜像 `ANCHORPROFILE` 机制);`--version` 分支 |
| 5 | ctc 驱动 sh 版 | 新 | POSIX | §4 全部子命令;build 负责发射 → 调 CC →(可选)链 c_src |
| 6 | ctc 驱动 PS 版 + cmd 垫片 | 新 | Windows | 同命令面;conformance 用例锁定两版参数兼容 |
| 7 | `_WIN32` 样板补丁 | `driver_emit.ct` | Windows | `main` 入口 `SetConsoleOutputCP(CP_UTF8)`(UTF-8 输出不被控制台代码页吃掉) |
| 8 | ~~lex `\r` 过滤~~ 已满足(双侧词法既有),本项仅 CRLF 回归夹具(已落 `tests/06_crlf.ct`) | `lex.ct` | 全(Windows 受益) | CRLF 源可解析;夹具以语言内自钉钉死(词法不再跳 CR 即编译拦截,CR 泄入字面量即 len/eq 断言炸)。否则记事本存个 CRLF 文件即解析炸 |
| 9 | 栈链接参数 | `ctc` 两版 + `native.sh` | Windows 为主 | §4 所列 bytes |
| 10 | `tools/release.sh` | 新 | 全 | 一键出 6 产物 + SHA256SUMS;本地可跑,CI 复用 |
| 11 | Release workflow(双阶段) | `.github/workflows/` | 全 | §6 |
| 12 | examples 就位 | `examples/` | 全 | ctgrep/ctwc/ctwf 作为发布验收与文档素材 |
| 13 | E5030 补齐 Static/Const | `parse_pkg.ct` | 全 | 碰撞比对现覆盖 Fn/FnPub/Struct/Enum/FnExt;用户 `static let` 撞导入符号会漏网落到环境遮蔽,补齐同检 |
| 14 | std 缺失告警分级 | `parse_pkg.ct` | 全 | 现状 std 模块缺失**静默跳过**(种子语料兼容)。分级:装机路径(②)或 `CTRON_STDPATH` 命中而缺失 → 告警(疑安装损坏/配错);回落路径(③)缺失维持静默 |

## 6. CI 与发布流程(双阶段)

**Stage 1(mac/linux 四平台 matrix)**:全链构建(`ci.sh` 口径)→ 预发射 C
**双平台发射 diff 逐字节验证**(固定点从内部不变量升级为发布承诺)→ 产物上传 artifact
→ 跑 §7 验收。

**Stage 2(windows-latest,MSYS2)**:`needs: stage1`,取预发射 C artifact →
**直接 `cc` 三件出 .exe(绕过 seed)**——Windows CI 走 cc-only 源码线,自举链验证留在
mac/linux;同时即验证了源码线在 mingw 下成立 → MSYS2 bash 里跑 §7 验收(sh 可用,
测试面统一)→ 打 zip → 汇总预编译五件 + src + SHA256SUMS 上 GitHub Release。

per-platform 发布容错:任一平台验收失败不阻塞其余平台过验收,但 Release 标记
`pre-release` 直至全绿。

## 7. 验收门禁(每个平台产物上 Release 前必过)

1. 解压即用:`ctc run` examples 三件输出正确;`ctc build` 出可执行且输出与解释一致;
2. **工作目录 ≠ 安装目录**用例(打 std 解析回归):任意 cwd 下 `use std.*` 命中装机路径;
   同名碰撞 `E5030` 负例拦截;装机路径 std 缺失产生告警(非静默);
3. 负例拦截 rc=1;`ctc --version` 正确;`ctc --help` / `ctc help build` rc=0 且内容
   含全部子命令,未知子命令 rc=2 并提示 `ctc --help`;
4. 源码线:`make` → 三二进制 → 同套用例绿;
5. **无 cc 预检**:PATH 隔离用例下 `ctc build` 仍产出 `.c` 且 rc=2、消息含平台指引与
   `.c` 位置;同环境 `ctc run` 不受影响。

Windows 专项(在 1–5 之上追加):

6. 并发夹具(spawn/Channel/Mutex,winpthreads 通路);
7. 深递归夹具(栈链接参数生效验证);
8. CRLF 夹具(第 8 项改造的回归);
9. 驱动 conformance:ctc.ps1 与 sh 版同参数面,`--help` 文本归一化后一致。

## 8. 边界(v0 明确不做)

- MSVC 后端(= 并发运行时重写)、windows aarch64;
- windows `make bootstrap`(自举链验证留 mac/linux;MSYS2 里 `make` 走预发射通路);
- Homebrew formula、winget/MSI 包(布局已留位);
- 包管理器 / 第三方 `use` 依赖解析;
- Developer ID 公证;ctc.cmd 冷门 shell 场景(引号转义差异等)的完全 parity。

## 9. 风险与缓解

| 风险 | 概率 | 缓解 |
|---|---|---|
| linux 侧从未实测,首跑暴露 cc/glibc 差异 | 高 | workflow 首跑即为验证轮;cc 参数口径收在 ctc/native.sh 单点 |
| winpthreads 并发行为与 mac/linux 不一致 | 中 | Windows 专项验收 5;β 标记兜底 |
| 深递归在 Windows 默认栈下段错误 | 高(已论证) | 第 9 项改造,验收 6 钉死 |
| GetModuleFileNameA 路径分隔符 | 低 | Win32 API 接受混用分隔符;验收 2 兜底;异常再归一化 |
| 固定点跨平台不逐字节(如 cc 版本差异影响发射文本) | 低 | 发射文本只由编译器源决定,与 cc 无关;Stage 1 diff 验证即证伪机制 |
| ctc 双驱动命令面漂移 | 中 | conformance 用例(验收 8)进两平台门禁 |
| 发布 workflow 首跑风险:macos-13 runner 退役可能、accept_windows 零执行史、linux 双臂 glibc 首跑 | 中 | 首跑即验证轮;per-platform 容错 + pre-release 标记兜底(§6),windows 运行级门禁归 CI job |

## 10. 后续挂账(v0.1 之后)

Ctron 自写原生 ctc 驱动(需运行时子进程能力)、Homebrew/winget 分发、
`install.ps1` 一键装、Developer ID 公证、windows aarch64、MSVC 评估、`ctc doc`/补全、
消除 `build` 的 cc 依赖(捆绑微型 C 编译器如 tcc,或内置机器码后端——大工程另立项)。

### 10.1 后期规划:std 预编译缓存(AST/字节码格式)

**触发条件**(满足其一才立项,当前均不满足):std 体量涨到每次编译的重复解析占比
显著;或出现无源码分发的场景需求(受限环境/商业分发)。

**形态**(届时另立 spec,此处记边界与关键判断):

- **产物**:安装时(或首次使用时)把 std 各模块的 parse 产物持久化为缓存文件
  ——模块 AST 的序列化,含行号戳(§9 批次的 span 语义)与模块依赖序;
- **消费点**:插在 `pkg_load_use`——命中缓存则跳过 read_file+parse,直接进语义面。
  关键判断:run/check/emit 三个驱动**共享 parse+sem 核心**,缓存位于驱动分叉上游,
  故"解释器和发射器都能消费"天然成立——是一个消费点,不是两处适配;
- **失效**:缓存键 = 工具链 VERSION + 模块源 hash,不匹配即重建;缓存是加速项,
  不是依赖项——缓存缺失时必须无感回落源码解析(源码线始终随包分发,§2.1);
- **明确不解决**:产物运行速度、二进制体积、源码保密——只解决编译期重复解析。

### 10.2 远期:模块限定访问(namespace)

现状定位:显式符号导入 + `E5030` 拦碰撞,已站 Rust/Java 档(碰撞=编译错误);
缺的是 **`json.parse` 式限定形式**——无限定导入把所有名字挤进单一平命名空间,
长期逼 std 用越来越长的方法名。方向:名字解析(sem E2020 调用解析/eval call_mem/
trans 调用点)增加"模块限定名"一路,use 的 Syms 挂到模块命名空间而非平铺进全局;
落地后无限定导入可保留为糖(碰撞面自然收窄)。与 §10.1 同层(都动 `pkg_load_use`
/名字解析),立项时同期评估。
