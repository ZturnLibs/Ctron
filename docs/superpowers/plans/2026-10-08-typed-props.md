# typed-props 组件实参 v1——struct 值穿 CTML 通道,真递归 tree 解锁

> 2026-10-08 立稿。来源=cf2132cf 登记(D 档 v2 尾巴③收尾时定罪:纯串通道不过
> struct)+ 路线图残余项(G 波二 GUI-37/44 已销账 8d930b80/a88dbe41 后唯一剩余)。
> 基线=1008 main 07221680(阶梯 90 过/1 败=s30 在册,suite 111 双列,smoke 232/0)。

## 0. 实证定罪(1008 探针 /tmp/tp_probe 四连)

1. **语言面**:`struct Node { label: Str, kids: List[Node] }` 自引用 chk/emit
   双臂绿(p1,递归计数 2 正确)——登记维持。
2. **GUI 缺口**:嵌套组件 struct 实参 `<Item n: {m}/>`——chk/emit 零错,
   运行期实参退化空串(p2,CHILD-A 丢失 cmds=1)——缺口本体重证。
3. **desugar 现状**:run(ViewCall) 合成 bind 对裸 struct 路径已生成
   `buf.push(__p_m)`(p3 C 码:prop:m 臂直推**活 struct 指针**)——契约Ⅰ
   魔数通道已用到 struct,push 侧就绪;缺的只有**消费侧字段读/列表迭代**。
4. **Box→Str 恒等豁免普遍成立**(p4):赋值/传参/push 三形 chk 绿,emit
   运行期指针逐字节保真——**无需任何 extern 恒等助手**,消费侧直接把
   buf 里的指针串当 `Box[T]` 用。

## 1. 设计定稿(路径视,1008 探针后终版)

**表示**:struct prop 值 = **根相对路径打标串** `\x02<rootprop>[.rel…]`(纯
文本,无句柄表无指针穿env);唯一类型化锚 = run(ViewCall) 合成 bind 闭包
捕获的 `__p_<prop>: Box[T]`(s49 既有)。消费 = 生成 per-struct **reader/
items**(布局感知下钻,字段路径→打标串 i:/b: 约定)经 bind 通道应答。

**探针定型的语言面约束**(全部实测):模块级 var(含 push-only)发射器
SIGSEGV;`下标[0].成员` 链发射臂不认;Str→Box 形参发射 C 不兼容;同名
extern 双声明末位胜(不可 per-T)。⇒ 消费面必须全程类型化于生成 Ctron 源,
通道只走文本——路径视为唯一满足全部约束的形。

**通道协议**(bind buf[0] 键,与既有 prop:/each:/when: 全前缀不撞):

| 键 | 形 | 应答 |
|---|---|---|
| `obj:<root>.<suffix>` | 根名路由 `__p_<root>` | buf 尾=reader(T, suffix) 打标串 |
| `eachobj:<root>.<field>` | 同上 | buf 尾=子节点 `\x02<root>.<field>.<i>` 逐项 |

env 值 `\x02m.kids.0` → bxv 组合查询 `obj:m.kids.0.label` → bind `obj:m.`
臂 → `__gui_ds_read_TNode(__p_m, "kids.0.label")` 递归下钻。

**消费形态(v1 终形,GUI-18 tree v2 目标语法)**:

```ct
struct TNode { var label: Str; var kids: List[TNode] }

view TreeItem(n: TNode) {
  <vbox class="ti">
    <label>{n.label}</label>            // obj 读:标量叶
    <each c in={n.kids}>                // eachobj:字段列表→子指针逐项
      <TreeItem n: {c}/>                // itemvar 直穿(既有臂零改)
    </each>
  </vbox>
}

view Root(m: TNode) {
  <vbox><TreeItem n: {m}/></vbox>       // prop:m 臂已推活指针(p3 实证)
}
run(Root(m: mk()))
```

终止=空 kids 列表 each 零迭代;`{n.kids.len > 0}` when 门=obj 读 `kids.len`
→ `i:N` 打标串(bxv 既有约定)。

## 2. 落点面

### A. 编译器 desugar(compiler/src/gui_parse.ct)

1. **struct 闭包收集**:run 视图 struct-typed props(ptyps head∈stns)出发,
   sfk/stv 字段表传递闭包(visited 防环,递归 struct 天然收敛单 reader)。
2. **per-struct reader 生成** `fn __gui_ds_read_T(p: Box[T], path: Str) -> Str`:
   - Str/I32/Bool 字段:`path == "f"` → 原样/`i:`/`b:` 打标;
   - 标量 List 字段:`f.len` → `i:N`;`f.N` → 元素(下标越界回落 "");
   - struct 字段(U 值/Box[U]):`f` → `Box[U](p.v.f)`/`p.v.f`(指针串,
     p4 豁免);`f.rest` → 递归 `__gui_ds_read_U(...)`;List[T] 字段:
     `f.N` → 元素指针串、`f.N.rest` → 递归;
   - 未识别回落 ""(通道口径 fail-soft);Optional 字段跳过(v1 边界登记)。
3. **分发臂生成**:bind 首臂插 `obj:`/`eachobj:` 分发(按 T 前缀链,调
   reader/items);键与既有 prop:/each: 全等比较互不干扰。
4. **eachobj items 生成** `fn __gui_ds_items_T(buf, p, field)`:顶层 List
   字段逐项 `buf.push(p.v.f[gi])`(元素指针串)。
5. **根级 struct 列表 each**:`each x in={m.kids}`(Root 体,env miss 落
   bind)——lists 收集带 lty,struct 元素列表分支 push `Box[T](elem)`。

### B. gui 域包运行时(pkgs/gui)

1. `bxv_ask` props 根分支:env 命中值 byte0==2(marker)→ 组合
   `obj:<rel>.<suffix>` 查询(rt_bind_obj;`path==root` 裸 struct 回 "")。
2. `rt_bind_items`:bind 回落前插 marker 根组合(`eachobj:` 通道,
   rt_bind_obj_items 应答多项);非 marker 值零改动。
3. 新助手 `rt_bind_obj`/`rt_bind_obj_items`(非 pub,rt_bind_text 同形)。
4. `.len` 臂零改动:全路径 `n.kids.len` 走 obj 读回 `i:N` 打标串。
5. **零新状态**:无表无纪元无 C 侧改动(路径视设计红利);每帧从活树
   重导出,跨帧悬垂不存在。

### C. 检查面(compiler 同文件)

v1 不动:实例 prop 实参纯路径性由 E8195 形态门既有覆盖(表达式禁调用),
型别匹配门(实参型 vs 声明型)随 vr 表型别穿线另件登记(§3 边界)。

## 3. 边界登记(v1 不做)

- each 项 struct 指针的点链读(`{c.label}`,c 为 eachobj 项)——项无型别
  线,递归 tree 形不需要(项直穿组件);候 itemvar 型别穿线另件。
- struct 值位 List prop 实参(`kids: {m.kids}` 实参位字段列表)——通道
  v1 回落 "";用组件内 each 形替代。
- Optional 字段跳过(reader 不认);sk 直通树(test_sk)env NULL 门控,
  typed 面不生效(gt_parse 生产路径全量生效)。
- 事件实参 struct 传递(ev_arg 族)不扩——事件面仍打标串。

## 4. 验收

- **夹具先行**:s92_typedprops——三节点 TNode 树递归渲染,headless 断言
  三层 label 全现+嵌套几何 x100 累进(沿 s73 配方)+ when 门剪枝路径;
  run.sh 阶梯注册行补占(实测 s92 空槽)。
- **双臂**:chk 零错+emit 阶梯绿;ctron-cc interp 臂(native.sh 已链 gui
  shim dlsym 桥)首选加验,不可行则如实记录单口径。
- **回归**:tests/gui/run.sh 全阶梯(90 过/1 败基线)→ suite → smoke;
  decls 锁/COVERAGE 本记随件。

## 5. 风险

- desugar 生成文本走 scan4/p_file 真解析——`{` 字面量 `\{` 转义(gui_ds_gen
  同款);生成源含 while/嵌套 if,拼装逐段 grep 锚点(大文件手术坑在册)。
- env_t 三表平行截断:漏一处=跨帧串型泄漏(一帧滞后冻结,s55 同族)。
- List[T] 元素 push 装箱(Box[T](elem) 拷贝)——渲染纯读安全;GC churn
  自适应阈值在册(T31-M15)。

## 6. 实现终记(1008 落地校准,规划→实现的四处偏差)

1. **`\}` 转义三犯**:Ctron 串字面量 `\}` 非法(E1001,SB 批血坑六犯再犯
   两次)——生成段闭合括号一律裸 `}`;`-> Void` 返回注解非法(声明位 Void
   不写,fn 类型位才用)→ items 落 `-> Void` 生成即 C 端 int32+`return;`
   错配。
2. **items 需 base 参**:marker 必须携带**根全径**——drill 后 path 已短,
   `root+path+gi` 在二层起错指父节点(实测=深度2 each 自旋返回父 marker,
   无限递归 stack-overflow)。定形=items 增第 5 参 base(入口=查询全径,
   drill 原样下传,marker=root.base.gi)。
3. **reader 需 struct 元素下标递归臂**:初版裁掉后深径(`kids.0.label`)
   断链(探针实证 buf 无应答)——补 `__gui_ds_read_U(Box[U](p.F[i]), rem)`
   (下标拆分:首 dot 前整数,余径递归)。
4. **几何断言改文本 x100**:Clay 只对有绘制元素发 RECT,裸容器不发——
   s73 的 w100 容器断言前提在此不成立;文本命令 x 已编码嵌套(padding 逐层
   右移),按文本宽唯一寻址断言。

## 7. 验收记录(emit 臂)

- s92_typedprops 全绿:三层树 TOP→MID→GKID-C+侧枝 KID-B 五文本断言过,
  嵌套几何 x100=1400/2000/2600 严格递增,终止后 DONE92 照常。
- 语言面探针存档(/tmp/tp_probe p1–p16):模块级 var(含 push-only)发射
  器 SIGSEGV;`下标[0].成员` 链发射臂 E 报;Str→Box 形参发射 C 不兼容;
  同名 extern 双声明末位胜;Box→Str 豁免(p4)与 List[Box[T]] 直取
  (句柄表方案弃用根因=p12 静默空产物)。
- 全门回归待 seed 重建完成后执行(阶梯/suite/smoke)。
