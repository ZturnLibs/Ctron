# G2 dlopen 补完 实施计划(全局域加载 / 句柄 thunk / 链接旗标真消费 / 真 .so 夹具)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 秘本 P0-1 落地——动态加载大型 C 库的三缝补完:dlopen 全局域形态(dlopen_global,解 RTLD_GLOBAL 缺失=#[dlsym] thunk 找不到 dlopen 库符号)、`#[dlsym(from_handle)]` 句柄 thunk(句柄式符号解析,sherpa-onnx 接入形态)、`#[link]` 旗标从「打印」变「进 cc」(三链接行统一+search/framework/env 逃逸口),真 .so 夹具双形态验证。

**Architecture:** 全部落在 `compiler/src/trans_expr.ct`(dlopen 族内建臂,1511 区)、`compiler/src/driver_emit.ct`(#[dlsym] thunk 臂 872 区/Const/模板)、`compiler/ctc.sh`(旗标消费+真 .so 夹具专道)、`compiler-c/src/rt_eval.c`(C 宿主镜像;interp 拒绝面维持=文档化),语法零新增(`#[...]` 属性堆叠机制增殖)。

**Tech Stack:** Ctron 自举编译器+dlfcn(darwin dlopen 无 RTLD_ONLY 无碍;NOW|GLOBAL 组合)+ cc -shared。门:tests/ffi dyn_so 专道、既有 ffi 族、suite、smoke ffi 块。

## Global Constraints

- Ctron 无位运算算符:RTLD_NOW|RTLD_GLOBAL 以**独立内建**表达(dlopen_global),不做 flags 形参。
- 解释口径(自举 CVM/C 宿主 interp)对 dlopen 族维持拒绝(eval_call/rt_eval 现状),错误文案统一「动态加载仅发射档」;夹具一律 emit 驱动。
- wasm/bare 维持硬拒(ctc.sh 现行 fail-closed)。
- 三处注册惯例+pathspec 限定提交+`.cache/emit` 清缓存真跑(恒例)。

---

### Wave 1: dlopen_global + dlerror_str 内建

**Files:**
- Modify: `compiler/src/trans_expr.ct`(1511 区两臂)、`compiler/src/sem_calls.ct`(prel)、`compiler/src/eval_call.ct`+`compiler-c/src/rt_eval.c`(拒绝面文案对齐)
- Create: `tests/ffi/dyn_so/`(专道,W3 补真 .so;本波先负例腿:坏路径 dlerror 面经 emit)

**Interfaces:**
- Produces:`dlopen_global(path: Str) -> I64`(发射 `RTLD_NOW|RTLD_GLOBAL`,返回句柄 0=败)、`dlerror_str() -> Str`(dlerror 非空→arena 拷贝串;空→空串;调用时机=失败后立即)。
- 语义:dlopen_global 加载的库符号进全局命名空间 → 既有 `#[dlsym]` RTLD_DEFAULT thunk 可达 → 「dlopen 后 #[dlsym] 直调」大库接入形态成立。

- [ ] Step 1: 双内建三处注册+模板臂(C 宿主 interp 拒绝文案)
- [ ] Step 2: dyn_so 负例腿(坏路径句柄 0+dlerror_str 非空判词;emit 驱动)*先红后绿*
- [ ] Step 3: 门:ffi 族+suite;Commit

### Wave 2: #[dlsym(from_handle)] 句柄 thunk

**Files:**
- Modify: `compiler/src/driver_emit.ct`(#[dlsym] thunk 臂 872 区:属性含 from_handle → 发射 `ct_dynh_<名>(I64 h, ...)` 逐调 `dlsym((void*)h, "<名>")`,首调缓存按句柄不安全=不缓存,登记)
- Modify: `compiler/src/trans_expr.ct`(调用点改写 ct_ext_sym 面:from_handle 形态首参透传)
- Create: dyn_so 正例腿(sherpa 形态:`let h = dlopen_global(...)` + `#[dlsym(from_handle)] fn api(h: I64, ...) -> I64;`)

**Interfaces:**
- Produces:`#[dlsym(from_handle)] extern "c" fn <名>(h: I64, ...) -> T;`——首参=句柄,thunk 逐调解析(句柄失效=ctron_panic 响亮);与 W1 dlopen_global 组成大库接入全形态。

- [ ] Step 1: thunk 臂扩展(属性解析+发射形)+调用点透传
- [ ] Step 2: 正例腿(真 .so 待 W3;本波以系统库 libz.dylib/zlib so 探针验证句柄解析)+门
- [ ] Step 3: Commit

### Wave 3: 链接旗标真消费 + 真 .so 夹具

**Files:**
- Modify: `compiler/ctc.sh`(emit_link_summary 从「打印」升级「收集」:`// ctron:link` 与 `// ctron:ldflag` 注释收集进链接行;`CTRON_LDFLAGS`/`CTRON_CFLAGS` env 逃逸口;根 ctron 驱动链接行同面)
- Modify: `compiler/src/driver_emit.ct`(`#[link(search = "dir")]`→`// ctron:ldflag -Ldir`;`#[link(framework = "名")]`→`// ctron:ldflag -framework 名`)
- Create: `tests/ffi/dyn_so/`(专道:harness `cc -shared` 出插件 .so → dlopen_global+#[dlsym] 与 from_handle 双形态断言 + 坏路径负例;参照 link_math 专道形制 run.sh:134-151)

**Interfaces:**
- Produces:用户程序 `#[link(name)]`/`#[link(search)]`/`#[link(framework)]` 三形态真实进链接行;env 旗标直通(rpath/-Wl, 任意);tests/ffi/dyn_so 双形态门。
- 挂账:根 ctron 驱动(原生 W4 波1)链接行同面随其波次;ctc.sh 项目模式 c_src 自动收集随构建驱动器 lane。

- [ ] Step 1: ctc.sh 收集+消费+env 逃逸;三链接行对齐(ctc.sh 先行,根 ctron 登记随批)
- [ ] Step 2: 属性 search/framework 扩展(driver_emit 注释发射面)
- [ ] Step 3: dyn_so 专道(cc -shared 插件;双形态+负例)双宿…单臂 emit 驱动
- [ ] Step 4: 门:ffi 族+suite+dyn_so;Commit + COVERAGE 批终

---

## 验收对照(设计 §4)

| 设计缝 | Wave |
|---|---|
| dlopen_global(RTLD_GLOBAL 缺口) | W1 |
| dlerror 诊断面 | W1 |
| #[dlsym(from_handle)] 句柄 thunk | W2 |
| 旗标真消费(三链接行)+env 逃逸口 | W3 |
| c_src 自动收集(ctc.sh) | W3-lite |
| 真 .so 夹具(dyn_so 双形态+负例) | W3 |
