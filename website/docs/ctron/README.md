# ctron 包 —— 编译器插件接口包

插件 = 普通 Ctron 包,两类约定入口:

- **derive 插件**:`pub fn ctron_derive(input: DeriveInput) -> Str`
  (返回合成 Ctron 源码文本)
- **lint 插件**:`pub fn ctron_lint(unit: LintUnit) -> List[LintDiag]`
  (返回 W9xxx 诊断;码须在插件清单 plugin 块 codes 中先进表)

编译器在本包声明与自身值镜像间做**字段名序核对**,不符 =
`E5060.iface`(fail-closed)。v1 冻结面:加字段 = 破坏性扩展,须走修订程序。

参考页:[plugin.md](plugin.md)(DeriveInput/LintUnit/LintDiag 全字段,
签名面自动生成)。

沙箱三层与协议细节以本包 `pkgs/ctron/README.md` 与插件示例(`tests/plugins/`)为准。
