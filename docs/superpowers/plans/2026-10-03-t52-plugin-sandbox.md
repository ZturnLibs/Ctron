# T52 实施计划(红锚先行;设计案见 specs/2026-10-03-t52-plugin-sandbox.md)

前置对齐:main @ 7b5f2feb;对端未提交编辑(`compiler/src/trans_expr.ct`、`tests/gui/s33_focus/src/main.ct`)不碰,提交 pathspec 限定。

## 批次与红锚

### 批1 清单协议 + schema + 接口包(零编译器行为变化)
- `tools/ctcl_manifest_schema.ctcl`:`reg "plugin"`(keyed,`name_pattern = "[a-z]+\\.[A-Za-z][A-Za-z0-9_]*"`)+ regkey `plugin.path`(str, required)/`plugin.codes`(list)。
- `tools/ctcl_check.py` selftest 若需样例补样例;全仓清单回归绿。
- `compiler/src/parse_pkg.ct`:`fn pkg_plugins(dir, diags) -> List[Str]`——文本扫描(同 `pkg_caps_allowed` 形):平表每插件 3 槽 `[fullkey, path, codesJoined]`(derive 形 codesJoined="")。
- `compiler/src/diag_msg.ct`:E5060 双语。
- `pkgs/ctron/plugin/plugin.ct`:接口包 v1 冻结面(6 struct,设计 §2.2)。

### 批2 沙箱底座(纯度门 + 执行器 + 预算)
- 新文件 `compiler/src/plugin.ct`(build.sh 拼接自动含;确认拼接规则)。
- `plugin_load(dir, relpath, diags) -> List[Str]`:读 `<path>/src/main.ct` → scan4+p_file → pkg_load_use_t(use ctron.plugin 走既有解析链)→ 纯度门(见下)→ 返回插件全 file。包规模门:源码 ≤256KiB / decl ≤512 → E5060。
- 纯度门 `plugin_purity`:全 fn 过 sandbox 白名单(comp_banned_call ∪ now_ms/now_ms_text/fs_*/Env/str_from_c ∪ comp_banned_mem)→ E6020.effect;`sem_cap(file, d, "E6020")` 复用(E6020.cap);`extern` 节点 → E6020.extern(新子码,diag_msg 登记)。
- 执行器 `plugin_call(pfile, fnname, arg: Val) -> Val`:`find_decl` 取形参 → `call_id(pfile, env0, "", fnname, [v_box(arg)])`;kind!="k" → E5060 族。
- 预算:`Global[Bool]` 激活开关 + `Global[I32]` 计数,`eval_call`/`run_stmt` 入口插桩(开关假直通);4096 步/入口调用,超限 println E6010 后 panic(v1 已知边界:JSON 模式格式破坏登记)。

### 批3 derive 展开钩子(三驱动)
- `plugin_expand(file, dir, diags) -> List[Str]`:扫 Struct/Enum 槽[2] Drvs → 内建 {Show,Eq,Error} 直通 → 查清单 derive.<名>(缺 = E5060)→ plugin_load → 构造 DeriveInput Val(AST Field 槽[1]/[2]/[0])→ plugin_call → 产物文本 scan4+p_file → 约束门(无 Use/无 Drvs/无 test,违 = E5060)→ 注入主 file 尾。
- 挂点:`driver_check.ct`/`driver_run.ct`/`driver_emit.ct` 的 `pkg_load_use_t` 之后;diags 非空 → 打印 rc=1。
- 红锚:`tests/plugins/no_decl/`(E5060)先行。

### 批4 官方 derive Json 插件 + 端到端
- `compiler/plugins/derive_json/{Ctron.ctcl, src/main.ct}`:字段面 Str(转义 helper 生成)/I32/I64/F64/Bool;标量 to_string 拼接;生成源码文本注意 `\{`/`\"` 转义。
- 红锚:`tests/plugins/json_demo/`(@derive(Json) struct + to_json 断言,run+emit 双臂)。

### 批5 lint 管线 + 官方样例 + 确定性
- `plugin_lint(file, dir, warns)`:清单 lint.* → plugin_load → 构造 LintUnit(fn 签名/attrs/stmts/depth 从 AST 计)→ plugin_call → LintDiag 解构 → codes 过滤(码 ∉ 清单 codes = 丢+一次性警告)→ `PATH:line: W9xxx: msg`(不置 rc)。
- check 驱动 sem 全绿后挂;run/emit 不挂 lint(v1)。
- `compiler/plugins/lint_toolong/`:stmts > 50 → W9001。
- 红锚:`tests/plugins/lint_demo/`(W9001)+ 确定性双跑 diff 空。

### 批6 阶梯挂 ci.sh + meta_check
- `tests/plugins/run.sh`:json_demo run 断言 / lint_demo check 断言 / sandbox_escape E6020 / no_decl E5060 / 确定性两连跑;ci.sh 插一步(第 5 门后)。
- `tests/meta_check.py`:ERROR_CODES + E5060/W9001(锚指 tests/plugins/)+ W9xxx 清单 codes 背书口径。

### 批7 规范注 + 台账 + 落库
- spec §8.3/§10.6 修订注、README 注册表(E5060/W9001)、COVERAGE、divergences parity 债、总计划台账回写、记忆。
- 门禁:ci.sh 全绿 + `compiler/test/suite.py` 双跑 + decls 锁实测回写(GUI 泳道在册口径)。
- commit:pathspec 限定(勿卷对端 trans_expr.ct/s33_focus)。

## 已知风险位(实施中证伪)
- `use ctron.plugin` 在原生臂/安装态的 site-root 探测(ctron_smoke 口径)——批3 实跑验证。
- Global[T] 在编译器源码内做预算计数器的写法(读/置值语法)——批2 先写 3 行探针验证。
- impl 固有方法在 eval/发射两臂对合成产物的消费面(合成 impl 走既有 Impl 检查/发射,预期零新机制)。
- 三线 parity:plugin 块 C/Rust 校验器、接口包 use 解析在 C/Rust 线 = 登记债。
