# T52 插件沙箱体系:derive + lint(v1 设计)

- 日期:2026-10-03。规范锚:§8.3(注解契约)、§10.6(插件扩展点)、§12.5(`@derive(DbRow)` 志向档)、§8.4(parametricity/E6030,类型反射仅经 `@derive` 声明)。
- 总计划卡:`docs/superpowers/plans/2026-09-28-spec-gap-execution-plan.md` T52(预估 3–4 d;前置 T25 已完成)。
- 横切裁决对齐:「能力优先于 hack」——本设计不设降级/替身路径;「HIR 暴露宽度」裁决在 §4 落定,按卡内要求先设计后动码。

## 0. 一句话

插件 = **普通 Ctron 包**;编译器在自身进程内以受限调用面加载并执行它(复用自举编译器内置解释器 = comptime CVM 的全量形态),derive 插件返回**合成 Ctron 源码文本**(普通代码,非宏手术),lint 插件返回 **W9xxx 诊断**。不引入进程外插件、动态链接或 FFI。

## 1. 架构:进程内 CVM 复用

```
用户包 Ctron.ctcl                    插件包(普通 Ctron 包)
  plugin "derive.Json" {               use ctron.plugin
    path = "../plugins/derive_json"    pub fn ctron_derive(input: DeriveInput) -> Str
  }                                      └─ 返回合成源码文本(impl 块)
  @derive(Json)                        pub fn ctron_lint(unit: LintUnit) -> List[LintDiag]
  struct Pixel { ... }                   └─ 返回 W9xxx 诊断
        │
        ▼  编译器(自举程序,check/run/emit 驱动)
  ① pkg_load_use_t(主包加载)后:plugin_expand 钩子
  ② 读清单 plugins 段 → pkg_load 加载插件包(独立 AST,不并入主 file)
  ③ 静态纯度门(§5)——加载期拒绝,启动期失败优于运行期越权
  ④ 构造输入 Val → call_id(内置解释器)执行约定入口 → 取回 Val
  ⑤ derive:产物文本 → scan4+p_file 重parse → 注入主 file 尾 → 正常 sem/发射
     lint:check 驱动 sem 全绿后执行 → 诊断按 W 级打印(不置 rc,镜像 E8193 口径)
```

- **CVM 复用的实义**:编译器源码里的 `eval_*`/`call_id` 解释机器既是 `ctron run` 的执行器,也是插件执行器;同一台机器、同一套值表示(Val),`interp/emit 同判` 天然成立(插件执行只发生在编译期,不进产物)。
- **与 comptime ceval 的关系**:`sem_ceval.ct` ceval 是 I 域白名单求值器(预算承载);插件需要字符串/表操作,走全量 eval 机器,**预算另挂**(§5.2)。ceval 白名单机制 = 本设计的思想底座,非代码复用。

## 2. 插件协议

### 2.1 清单注册表(CTCL,D7 注册表增殖、D11 键控块,文法零改动)

```ctcl
pkg { manifest_version = 1, name = "demo", version = "0.1.0" }

plugin "derive.Json" {
    path = "../../plugins/derive_json"
}

plugin "lint.toolong" {
    path = "../../plugins/lint_toolong"
    codes = ["W9001"]
}
```

| 块 | 形 | 键 | 类型 | 约束 |
|---|---|---|---|---|
| plugin | 键控,名 = `"<kind>.<name>"`,kind ∈ {derive, lint},重复名 = E5040 族 | path | Str | 必填;相对清单所在目录 |
| plugin | | codes | List[Str] | lint 形必填,derive 形禁止;成员须 `W9[0-9]{3}` 形 |

- 第三方码纪律(§10.6「先进表再使用」对第三方的投影):**lint 插件的码先进自己清单的 `codes`,再在代码/诊断中使用**。`tests/meta_check.py` 对 W9xxx 的检查口径同步:W9xxx 出现在仓库源文件时,须在 ERROR_CODES(官方)或任一 `Ctron.ctcl` 的 codes 中声明。
- schema 注册:`tools/ctcl_manifest_schema.ctcl`(单一事实源,数据驱动)加 `reg "plugin"` + regkey 三条——ctcl_check.py(CI 第 1 门)即认识 plugin 块,文法零改动(D7 兑现)。C 宿主(pkg.c)/Rust 参考(check.rs)校验器为硬编码镜像,plugin 块同步登记三线 parity 债(与 T35 同口径:自举线先落,C/Rust 线随批)。

### 2.2 接口包 `ctron.plugin`(编译器同仓官方包,解析链 pkgs 位)

```ctron
// pkgs/ctron/plugin/plugin.ct —— v2 面(2026-10-10 修订;加字段 = 破坏性扩展,走本节程序)
pub struct DeriveField {
    pub let name: Str
    pub let ty: Str        // 头标识符(v1 口径不动,既有插件兼容)
    pub let mutable: Bool
    pub let ty_full: Str   // v2:完整型别文本(Named+型别实参递归序列化,
                           //      Slice/Optional 渲染,其余空串)——List/Option
                           //      序列化前置;v1 的 ty 塌缩头标识符为已登记缺口
}
pub struct DeriveInput {
    pub let type_name: Str
    pub let fields: List[DeriveField]
}
pub struct LintFn {
    pub let name: Str
    pub let params: List[Str]   // "name:ty" 编码
    pub let ret: Str
    pub let attrs: List[Str]    // pure/no_alloc/no_spawn/trusted
    pub let stmts: I32          // 顶层语句计数
    pub let depth: I32          // 体最大嵌套深度
}
pub struct LintUnit {
    pub let module: Str
    pub let fns: List[LintFn]
}
pub struct LintDiag {
    pub let code: Str           // W9xxx,须在插件清单 codes 中声明
    pub let line: I32
    pub let msg: Str
}
```

约定入口(编译器按名调用,普通 `pub fn`,非 trait 方法——v1 零动态分发):

- derive 插件:`pub fn ctron_derive(input: DeriveInput) -> Str`
- lint 插件:`pub fn ctron_lint(unit: LintUnit) -> List[LintDiag]`

签名不符/缺入口 = 加载期 E5060 族诊断。编译器构造输入 Val 按 §2.2 struct 形态镜像(tag 8 复合:`["U", "DeriveInput", "type_name", box(Str), "fields", box(List)]`);构造前对接口包声明与编译器镜像做字段名序核对,不匹配 = E5060(fail-closed,防接口包被改字段序后静默错位)。

## 3. derive 展开管线

1. 展开点 = 三驱动(check/run/emit)`pkg_load_use_t` 之后共享钩子 `plugin_expand(file, dir, diags)`;fmt/doc 不展开(处理源文本,登记)。
2. 扫主 file `Struct`/`Enum` 槽[2] `Drvs`:名 ∈ 内建集 {Show, Eq, Error} → 现行声明性口径不动(v0 兼容,零迁移);其余名 → 查清单 `derive.<名>`;未声明 = **E5060**(`@derive(X):未声明 derive 插件(清单 plugin "derive.X" 缺席)`)。
3. 加载插件包 → 纯度门(§5)→ 构造 DeriveInput(AST 直读:Field 槽[1]名/槽[2]型文本/槽[0]可变)→ `call_id` 调 `ctron_derive` → 返回 Str。
4. 产物 = 合成 Ctron 源码文本 → `scan4`+`p_file` 重 parse → 注入主 file 尾。产物约束(违反 = E5060):不得含 `Use`;任何 decl 不得带 `@derive`/`Drvs` 非空(**禁递归 derive**);不得含 `test` 块。
5. 产物由正常 sem/发射管线消费——「普通代码,非宏手术」的验收即此:产物源码可被 `ctron check` 独立检查,人可读、可手写等价物。
6. 每次编译重展开(v1 无跨编译产物缓存;derive 确定性由 §5.3 锚覆盖)。官方样例落库实况:`@derive(Json)` → **自由 fn 形态**`fn to_json(self: T) -> Str`(UFCS 调用;**trait impl 形态弃用**——其发射臂为在册缺口[T25 v2 域],UFCS 双臂有 iter.ct/实测先例;值接收)。字段面 Str/I32/I64/F64(转义 helper 模块级生成,名带类型防撞)/Bool(if 表达式);不支持型别 → 插件返回空串 → 编译器 E5060.iface 空产物。`@derive(DbRow)` 不在本件(§12.5 志向档,运行时 rowmap 回落位已在库;插件化列后续泳道)。

## 4. lint 管线与 HIR 暴露宽度裁决

**裁决(v1):窄面声明级投影,不暴露编译器内部 AST。**

- 给插件的输入 = §2.2 `LintUnit`(fn 签名 + 注解 + 语句计数/嵌套深度)。**不给**:函数体表达式树、编译器 AST 节点原始形态、span 内部表示。
- 理由:①HIR 全暴露 = 把编译器内部表示冻结为公共 ABI,一切内部重构被插件生态锁死(演进权裁决);②声明级投影已覆盖 lint 纪律主族(命名约定/长度/深度/注解纪律/面计数);③窄面可渐进扩宽(加字段走 §2.2 修订程序),反方向收窄则破坏性。
- 表达式级 lint(如「禁 goto 式嵌套」)= v2 志向,单独立卡裁决,不在 v1 面内假装可用。
- 执行点 = check 驱动 sem 全绿后(错误先出,不级联);诊断格式 `PATH:LINE: W9xxx: msg`,W 级不置 rc(镜像 E8193);`codes` 外的码 = 丢弃并附一次性警告(第三方码纪律的运行期兜底)。
- 确定性与缓存:同输入同输出为硬锚(§5.3);诊断缓存 = 内容寻址(键 = 插件包源码 digest + LintUnit 投影 digest),落位复用 T20 emit 缓存基建形态(实施时评估复用成本,过贵则登记 v2 并只交确定性锚)。

## 5. 沙箱边界(三层)

### 5.1 静态纯度门(加载期,启动期失败)

插件包全部 `pub fn`(入口与助手)过:

- **I/O 白名单扫描**(E6020.effect 同构,`scan_comp` 白名单机制复用):print/println/read_file/read_dir/fs_*/Env → **E6020**;
- **能力调用门**(E6020.cap 同构,`sem_pure.ct` 判定机制):`&Cap` 接收者方法调用 → **E6020**;
- **extern 声明禁**:插件包任何 `extern` = **E6020**(FFI 即逃逸面,零豁免);
- **确定性白名单收口**:now_ms/now_ms_text/spawn/send/recv/store/fetch_add/with*(comp_banned_mem 全集)→ E6020。comptime 现行白名单不含 now_ms,是 comptime 面的既有口径;插件面为确定性硬锚**必须禁**,两白名单各自独立成表(插件表 ⊋ comp 表),不互相牵动。

诊断消息挂 `sandbox` 域注记(`E6020.effect(sandbox):插件包禁 I/O:println` 形),复用 E6020 主码——验收负锚「插件内触 I/O → E6020」逐字兑现。

### 5.2 预算门

- **v1 = 静态规模门**:插件包源码 ≤ 256 KiB 且 decl ≤ 512,超限 E5060.iface(防加载面滥用)。
- **执行期步数预算 = v2**(E6010 复用口径维持):语言内可变全局原语(`Global[T]`)经实证为**单绑定持久盒,跨调用不共享**(2026-10-03 探针,双臂同判)——编译器源码内无跨调用计数载体,eval 签名穿针属大手术。演进路径与 comptime 同款(v0 无步数预算、死循环挂起 check 为在册限制,sem_comptime.ct 头注先例;后随 T08 补步数预算)。插件纯度门(§5.1)已禁 I/O/时钟,死循环 = 编译挂起 DoS,登记 v1 已知边界。
- 插件包静态上限随 §5.1 加载期一并执行;诊断消息附预算域注记。

### 5.3 确定性

- 静态门已禁时钟/环境/并发源(§5.1);eval 机器同输入同输出。
- 锚:同一夹具 check/run 双跑输出逐字 diff 空(suite 内两连跑)。

### 5.4 E6030 关系

derive 展开产物是普通代码,插件在编译期以值面运行,不反射泛型运行时型别——§8.4 parametricity 保持不受扰动(T06 封闭性论证的 direct 推论,无需新机制)。

## 6. 官方样例(验收载体)

| 样例 | 位置 | 内容 |
|---|---|---|
| derive Json | `compiler/plugins/derive_json/` | `ctron_derive`:字段驱动生成 `fn to_json(self: T) -> Str`(自由 fn/UFCS;Str 转义 helper 模块级,名带类型) |
| lint toolong | `compiler/plugins/lint_toolong/` | `ctron_lint`:`stmts > 50` → W9001「函数过长」;codes=["W9001"] |
| 夹具族 | `tests/plugins/` | json_demo(端到端正锚)/ lint_demo(W9001)/ sandbox_escape(E6020 负锚)/ no_decl(E5060 负锚)/ 确定性双跑 |

W9xxx 段注册:`tests/meta_check.py` ERROR_CODES 增 W9001(官方样例码)+ W9xxx 清单声明检查口径;规范 §10.6/§8.3 挂修订注;`COVERAGE.md`/divergences 相应登记。

## 7. 明确不做(v1 边界,登记非降级)

- 进程外插件/动态库:永不在案(§8.3 非宏手术 + 沙箱单进程口径)。
- trait 化插件接口(v1 约定 fn):trait 对象动态分发进插件协议 = v2 裁决面;约定 fn 已满足「普通代码」且零 vtable 依赖。
- `@derive(DbRow)`:§12.5 志向档,运行时回落位在库,插件化列 db 泳道后续。
- 表达式级 lint 面:v2(§4 裁决)。
- R 线(ctronr)/C 宿主(ctronc)同步:与 T35 同口径登记 parity 债,自举线先落。
- derive 产物跨编译缓存:v2。

## 8. 实施序(TDD,红锚先行)

1. E5060/清单 plugin 段解析(`parse_pkg.ct` 文本扫描式,同 caps 形)+ 负锚 no_decl。
2. 接口包 `pkgs/ctron/plugin/` + 编译器 Val 构造/解构镜像核对。
3. 纯度门(白名单两表 + extern 禁)+ 负锚 sandbox_escape。
4. derive 展开钩子(三驱动)+ 官方 derive_json + 端到端正锚 json_demo(interp/emit 双臂)。
5. lint 管线(check 驱动)+ 官方 lint_toolong + W9001 锚 + 确定性双跑锚。
6. meta_check.py W9xxx 口径 + 规范修订注 + 台账回写。
7. 门禁:ci.sh 全绿 + suite 双跑 + decls 锁实测回写。
