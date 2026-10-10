# Ctron 构建驱动器升格与编排收编 —— 设计(任务面 / 门禁 / 清理;终态 = Ctron 自写驱动器)

- 状态:**方向已裁,设计定稿待评审**(2026-10-10 用户裁决"开始设计并规划";终态一句已获认可:驱动器 Ctron 自写、清单单一、shell 编排退场)。
- 日期:2026-10-10。
- 关联规范:配置语言 v1(`2026-09-16-config-language-v1.md`,下称 CTCL 规范;§3 工具配置框架、§4 冻结文法、D7 注册表演进)、工具链分发(`2026-09-17-toolchain-distribution-design.md` §4 CLI 面)、闭源包分发(`2026-09-21-closed-pkg-distribution-design.md` §7.2,本设计不改其旅程)。

## 0. 终态一句话

**`ctron` 成为一个用 Ctron 自写的原生二进制驱动器,`Ctron.ctcl`(包面)+ `ctron.ctcl`(驱动器面)两份清单管全部声明面,仓库约 190 份 shell 编排收敛到十份以内(剩 C 宿主引导与安装垫片)。** 驱动器只做编排,不做依赖图增量——增量所有权留在编译器的内容寻址缓存,make/bazel 形的通用构建系统是明确否决项。

## 1. 动机与证据(2026-10-10 快照)

现状不是"缺一个构建工具",而是"散装编排面没有所有权":

| # | 证据 | 数值 |
|---|---|---|
| E1 | 同构测试骨架复制:每个 run.sh 都是 `mktemp` + trap + pass/fail 计数 + grep 断言同一套 | tests/ 154 份 + examples/ 17 份 + compiler/selfhosted/tools 若干 |
| E2 | `pipefail` 全仓使用次数 | **0**(tests/ 154 份中 111 份 `set -eu`、43 份仅 `set -u`);在册血泪"管道假 rc 三犯"的根因即 sh 模型本身 |
| E3 | `CTRON_STDPATH` 手工 export | **130 处**;漏一处即 W8901 误红(在册坑) |
| E4 | emit 缓存(`.cache/emit`、`.cache/bare`、`.cache/wasm`) | 只有创建/命中,**无任何脚本负责清理**;"清缓存假绿"血坑的温床 |
| E5 | 链接咒语两形态并存 | `examples/todo/run.sh` 手写全套 GUI 链接参数 vs `gui_calc` 已走 `ctc build` 清单自动链接(J19-④) |
| E6 | 驱动器与测试家族的裂缝 | 根 `ctron` 已有 `test` 子命令,154 份 run.sh 没有一份使用——现有 test 面只覆盖单文件/测试块,不覆盖集成门 |
| E7 | 门禁编排硬编码 | `ci.sh` 九步串一条;逻辑内联(GC 确定性双跑 diff、gui 门平台分支)与调用混在一处 |

既有资产(升格的地基,不是推倒的对象):根 `ctron`(POSIX sh,16 子命令,rc 0/1/2 约定,T49 registry 与 T50 seal/attest/log 已实装)、`ctc.sh` 内容寻址缓存、CTCL 注册表机制(schema-as-CTCL 已落 `tools/ctcl_manifest_schema.ctcl`,且已随 T52 增殖过 `plugin` 表——"扩展 = 注册表增殖"的路径已被走过一遍)。

## 2. 裁决表(含否决记录)

| # | 裁决 | 否决的替代 |
|---|---|---|
| D1 | **不新造 make 类独立工具;升格现有 `ctron` 驱动器**——编排、门禁、清理全部收进同一入口 | 独立 `ctron-make` 二进制(第二入口 = 生态分裂);bazel/make 形文件依赖图(整程序发射模型下增量已由内容寻址缓存承担,错配) |
| D2 | **任务注册表立表为工具配置 `ctron.ctcl`**(根块 `ctron` + `task` 键控块),**不动 `Ctron.ctcl`**——CTCL 规范 §3 工具配置框架的正当落点:"消费者是谁、表就归谁立",驱动器是唯一消费方 | tasks 进包清单(三线解析器 C/Rust/自举连坐改表、编译器面与驱动器面混居、sealing/dep 消费面被迫容忍无关块);独立 YAML/JSON(破 D10 生态一致性) |
| D3 | **步骤执行 = 引号感知 argv 切分 + 直接 exec,无 shell 中间层**;shell 元字符(竖线、&、分号、$、反引号、<、>、圆括号、星号、问号)或换行在步骤串中出现 = 拒绝执行并给出去处(写脚本文件,步骤里调 `sh 脚本.sh`) | `sh -c` 中间层(管道假 rc 类缺陷不死、引号地狱、ctron.ps1 双宿主语义分裂);自创 DSL(违背"不发明新符号") |
| D4 | **gate 解析序:`ctron gate [名]` ≡ `ctron task [名]`,缺省名 = `gate`;解析 = 当前目录 `ctron.ctcl` 的 task 块 → 无文件或无此块时,项目模式(Ctron.ctcl 在场)回落内建默认门 `["ctron build", "ctron fmt --check ."]`,否则 rc=2 用法错** | 向上递归找清单(隐式作用域,违 fail-closed);gate 内建硬编码本仓九步(驱动器不得知道某个仓库的细节) |
| D5 | **`ctron clean` 默认 = 删工具链缓存三域(`.cache/emit`、`.cache/bare`、`.cache/wasm`);`--all` 加项目 `build/` 与 `pkgs/`(已安装依赖,项目模式)**;粗暴删除,正确性由内容寻址重建保证 | LRU/引用计数缓存 GC(YAGNI);构建前自动指纹校验失效(挂账,§9) |
| D6 | **终态:驱动器用 Ctron 重写为原生二进制 `bin/ctron`,根脚本降级为薄垫片**(垫片职责:定位并 exec 原生件,缺件时回落现有 sh 实现)——自举叙事收口:"Ctron 用 Ctron 构建 Ctron" | 永久 sh(假 rc 类/双宿主分裂不可根除、无法 dogfood 自家 std) |
| D7 | **分期 W1–W5**(§8),每期独立绿、独立落库,先立新面后收编存量 | 一次性大爆炸改写(190 份脚本一次翻,回归面不可控) |

**明确不做(挂账见 §9)**:步骤并行调度、步骤超时、缓存失效追踪、测试协议内建(目录即套件——需要独立设计轮,不在本设计拍脑袋)、对外 Makefile/CMake 导出、网络 registry。

## 3. 注册表二:工具配置 `ctron.ctcl`

### 3.1 键表

| 块 | 形 | 键 | 类型 | 约束 |
|---|---|---|---|---|
| ctron | 记录,必填,≤1 | config_version | I64 | **必须存在且 = 1**(沿 manifest_version 惯例;规范形态居首) |
| task | 键控,可选,名 = 任务名,重复名 = E5045 | steps | List[Str] | **必填**,非空(空列表 = E5054);元素 = 命令行(§5 切分) |
| task | | desc | Str | 可选;`ctron task`(无参)列任务时显示 |

诊断码全族复用 E5040–E5048/E5050(文法与通用校验注册表无关);`task` 专用新码一枚:**E5054**(steps 为空列表——"一个不干活的任务不存在");steps **缺失**走 E5051 既有码(与 member.path 同族:键控块缺必填键,schema `required_keys` 承载)。W5051(版本键未居首)语义平移到 config_version。E5054 的实现落点:schema 方言增一枚键级旗标 `nonempty`(列表键语义非空;进 `_REGKEY_FLAGS` 与校验器,反投机律满足——消费实现即本设计 §4 命令面)。

### 3.2 示例(本仓 W1 落地时的真实形态,缩样;列表单行是冻结文法钉子,长 steps 就是长行,fmt 归位)

```text
// ctron.ctcl —— 驱动器面:任务即数据(CTCL 规范 §3 工具配置;消费者 = ctron 驱动器)
ctron {
    config_version = 1
}

task "gate" {
    desc = "全量门(收编自 ci.sh)"
    steps = ["python3 tests/meta_check.py", "python3 tools/ctcl_check.py --selftest", "sh compiler/build.sh", "sh compiler/native.sh", "sh compiler/test/smoke.sh --full"]
}

task "smoke" {
    desc = "编译器冒烟单门"
    steps = ["sh compiler/test/smoke.sh --full"]
}
```

### 3.3 校验与工具面

- schema-as-CTCL:新增 `tools/ctcl_ctron_schema.ctcl`(`reg "ctron"` record + `reg "task"` keyed,`required_keys = ["steps"]`);加载器复用 `ctcl_check.py` 的 `_load_schema`,零改动。
- **注册表嗅探**:`ctcl_check.py` 按首个非注释块名选 schema(根块 `ctron` → 驱动器 schema;其余 → 清单 schema)——目录递归扫 `*.ctcl` 时两种文件不互相误伤(现 CLI 对未知根块一律按清单 schema 报 E5044,必须先修此点)。
- 黄金语料:`tests/manifest/` 增 `ctron.ctcl` 正负例(含元字符/空 steps/重复 task/缺 config_version),三处对拍口径不变。

### 3.4 sh 宿主读取器(W1 过渡形态)

根 `ctron` 脚本内置 awk 读取器(只认本注册表形;未知块/未知键/类型不符/缺 config_version = rc 2 + 逐条消息,fail-closed)。**登记为过渡债**:awk 读取器随 D6 原生驱动器落地退役,不追求与 E504x 消息逐字节一致(W1 验收只要求"拒收面"一致,消息一致性归 W4 对拍)。

## 4. 命令面增补

| 命令 | 语义 | rc |
|---|---|---|
| `ctron task` | 列当前目录 `ctron.ctcl` 任务(名 + desc;无文件 → rc 2) | 0/2 |
| `ctron task <名>` | 执行任务:逐步骤顺序执行,任一步骤非零即停,报"任务 %s 步骤 %d(rc=%d)+ 输出尾" | 0/1/2 |
| `ctron gate [名]` | ≡ `task`,缺省名 `gate`;含内建默认回落(D4) | 0/1/2 |
| `ctron clean [--all]` | 删缓存三域;`--all` 加项目 `build/` 与 `pkgs/`(已安装依赖) | 0/2 |

- 环境注入规则(驱动器统一施加于子步骤,取代 130 处手工 export):dev 布局(`<驱动器根>/lib/std` 存在)→ `CTRON_STDPATH=<根>/lib/std`;安装布局 → 不设(编译器内置解析)。用户环境原样透传;驱动器不提供步骤级 env 覆写(出路:脚本文件)。
- 步骤中裸名 `ctron` 解析为驱动器自身可执行路径(任务里写 `ctron build` 不依赖 PATH)。
- `ctron.ps1` / `ctron.cmd` 同文同步( house 规矩:三面同文)。
- `--help` / `help task|gate|clean` 与 usage 同步;rc 约定 0/1/2 不变。

## 5. 执行器语义(本设计的核心钉子)

1. **切分**:步骤串按空白切 argv;双引号包裹为单参,`\"`/`\\` 为字面引号/反斜杠(与 CTCL 字符串转义规则同形,一套心智);单引号不特判(就是字面字符——报错面给建议)。
2. **元字符拒绝**:竖线、&、分号、$、反引号、<、>、圆括号、星号、问号或换行任一出现 = 该步骤拒执行,消息点名步骤序号、元字符与两条出路(改写为多步骤 / 写脚本文件以 `sh` 调用)。**没有 shell 中间层,子进程 rc 由 waitpid 直接取得——"管道假 rc"一类缺陷在结构上不存在。**
3. **输出**:子进程 stdout/stderr 原样直通(不捕获不改写);任务失败时驱动器补一行判词(哪步、rc 几),不重复输出内容。
4. **cwd**:任务步骤一律以 `ctron.ctcl` 所在目录为工作目录(步骤串里的相对路径 = 清单相对,与直觉一致)。
5. 无超时、无并行、无重试(v0;挂账 §9)。

## 6. 缓存与环境治理

- 缓存三域路径与键机制不变(内容寻址:输入 + cc_emit shasum);本设计只收编**所有权**:创建/命中/清理三者同归驱动器,`ctron clean` 是唯一合法清理口。仓库脚本里此后不得出现 `rm -rf .cache`(收编完成后由 meta_check 增扫,见 §8 W2)。
- 失效追踪(键里加工具链指纹等)不进本期,挂账 §9——现状血坑("清缓存假绿")先由"clean 随手可得"缓解。

## 7. 终态架构:Ctron 自写驱动器(D6 展开)

- **源树**:`compiler/driver/src/main.ct` 起(与 compiler/src 平级,独立树;经 `native.sh` 编译入 `compiler/bin/ctron`)。
- **前置件三件(std 泳道,可独立推进,本身就是有价值的 std 面)**:
  1. 用户态**写 OS 环境变量**(`env_set`;现 env 只有读——子进程注入必需);
  2. **递归目录遍历**(`fs` 内建有 `read_dir` 无 walk;测试发现/缓存清理必需);
  3. `lib/proc` **门面补齐**(`ctron_proc_run_argv_p` 无门面转发;argv + rc + 输出捕获直取必需)。
- **CTCL 通用读取面**:std/config.ct 升 L1 最小 API(现硬编码清单注册表):`config_parse(src) -> Result[ValueTree, List[Str]]` + `blocks(名)` + `get_str/get_list`,注册表作参数传入——驱动器据此消费 `ctron.ctcl`,自举线 `ctcl_chk.ct` 镜像同步纪律不变。
- **分批与对拍**:按子命令族分批重写(run/check/doc → build/test/fmt/lint → add/publish/lock/pkg → task/gate/clean);每批验收 = 与 sh 实现**黄金对拍**(同命令同语料,stdout 与 rc 逐字节一致)+ 现有门全绿。
- **垫片翻转**:全部子命令对拍齐后,根 `ctron` 脚本头部改一行——原生件在场即 `exec`,缺件回落 sh 实现;install 布局由 Makefile 源码线增 `bin/ctron`(prebuilt 家族加一员)。
- **自举梯**:`selfhosted/bootstrap.sh` 阶梯增加驱动器一级(与编译器同梯);`ctron gate` 收编后,编译器仓库自身成为 dogfood 首例。

## 8. 分期(W1–W5)与验收门

| 期 | 内容 | 验收门 |
|---|---|---|
| W1 任务面(sh 宿主) | 注册表立表(schema + 嗅探 + 语料);根脚本增 `task/gate/clean` + awk 读取器;repo 根 `ctron.ctcl` 定义全量门(收编 ci.sh 九步,内联逻辑两处抽为脚本文件);ci.sh 翻转为一行 `exec ctron gate`;ps1 同文 | `ctcl_check --selftest` 绿 + 新语料绿;新 `tests/tasks/run.sh`(task/gate/clean 正负例)绿;**CI 在翻转后全绿**;fmt 净 |
| W2 收编开闸 | 试点域 run.sh 改由 task 表驱动(2–3 个形态最简域,如 examples 子集 + tests/plugins);meta_check 增扫"脚本私清缓存";usage/文档同步 | 试点域原门全绿(run.sh 删除或降为一行委派);meta_check 绿 |
| W3 std 前置件 | §7 三件(env 写 / 递归 walk / proc 门面),各自独立批次独立绿 | 各自模块门 + smoke 全绿(标准泳道口径) |
| W4 原生驱动器 | std/config L1 通用面 → driver 骨架(run/check/doc)→ 逐族对拍 → 垫片翻转 | 与 sh 实现黄金对拍逐字节一致(全子命令语料);现有全门绿;翻转后 dev+install 双布局冒烟 |
| W5 收编收官 | 残余 run.sh 逐域退役;shell 面盘点 ≤10(bootstrap/安装/薄垫片);文档与 tour 同步 | 全门绿;run.sh 存量收敛至白名单(C 宿主引导/安装/垫片);CI 绿 |

顺序弹性:W3 与 W1/W2 无依赖,可穿插;W4 前必须 W3 齐。

## 9. 开放项 / 挂账

1. **测试协议内建**(目录即套件、期望格式、断言协议):独立设计轮,不搭车本设计——154 份 run.sh 远非同构,E1 的"同构骨架"只是壳,内核断言面逐域异构。
2. 缓存失效追踪(工具链指纹入键)。
3. 步骤并行 / 超时 / 重试。
4. tasks 块是否最终并入 `Ctron.ctcl`(待三线注册表数据驱动化后重议;届时 D2 否决理由消解)。
5. 对外 Makefile/CMake 导出(`ctron export`;ffi-analysis 在册项)。
6. E5054 是否需要配套 W 码(如步骤含元字符的静态预警)随实现定。

## 10. 与既有规范的关系

- CTCL 规范:零文法改动(§4 冻结兑现);走 §3 工具配置框架 + D7 注册表演进 + 反投机律(消费实现 = 本设计 §4 命令面);诊断全族复用,仅增 E5054 一枚。
- 工具链分发 §4 CLI 面:本设计为其"驱动器"方向的展开,rc 约定与帮助面沿用。
- 闭源包分发 §7.2:`pkg` 族子命令原样随迁 W4 对拍,旅程不变。
