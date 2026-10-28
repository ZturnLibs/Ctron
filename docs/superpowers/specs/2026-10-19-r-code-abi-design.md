# R 码 ABI 设计轮——扁平 Option/Result 码携带元素码(2026-10-19 勘定)

> 来源:IP-01 W2a/W2b 错误链批(6a66efa5/e7a624ad/bdff5643/5d5fc4d7)连环比勘;
> 扁平 R 族三成员新立(5d5fc4d7)后的专门设计轮。状态:**设计稿,未排期**。
> 执行前必读:lib/std/fs.ct read_file、compiler/test/fx_optstr.ct(尾值 match 金源)、
> trans_stmt.ct R 分派臂(1308-1530)。

## 1. 现状勘定(全部实证)

### 1.1 表示分裂地图

Option/Result 的 C 表示按**生产者**分裂为三族,match 分派按**旁观式代码**跟随:

| 族 | 代码 | C 表示 | 生产者 |
|---|---|---|---|
| NULL 模型 | `"s"` | `const char*`(NULL=None) | read_file/read_line/…(内置 extern 面);typeof 硬编码 trans_ty.ct:1732 |
| ct_res | `"R"` | `ct_res{variant,v}` | Ok/Err/Some 构造(trans_expr 构造臂)、send/recv/join_or、context() |
| 通道 hR | `"hR<ec>"` | 堆盒指针 | Channel[T] 元素为 struct 时 recv(trans_stmt 1388-1400 特判) |

### 1.2 扁平 R 族成员(全部探针实证)

1. **Option/Result 作用户 fn 形参** → 塌缩 `int32_t`(ct_ctype("R") 占位;调用点传 ct_res 结构=编译错/静默错位)。
2. **注解 `Option[Str]` 本地位 let** → 同上(LET 臂按注解码取 ctype)。
3. **`Str?` 用户 fn 返回** → fn 签名 `int32_t` 而 `return Some(...)` 产 ct_res=不配;NULL 模型要求 `char*`。
4. **`Err("boom")` 幻影码** → ct_err_elem_of 初版手拼 `"E:Str"`(已修,5d5fc4d7;正形=ct_ty_code)。
5. **match R 臂非块体语义两制**(勘定保留):SOME/NONE=return 位(尾值 match 值臂实现,fx_optstr 金源)/OK/ERR=语句位(W2a)。语句位 match 的非块值臂=提前返回误用面。
6. **`?` on NULL 模型操作数**(read_file(p)?)→ Try 臂按 ct_res 解包,对 char* 操作数=未定义(现网无此用法,latent)。

### 1.3 已有可复用机制

- **`#elem:<名>`/`#cerr` env 侧条目族**(IP-01 批落地):绑定点的元素码侧条目,env 自然贯穿。
- **`ct_err_elem_of`/`ct_opt_elem_of`**(trans_ty):从返回型节点提 Ok/E 侧码。
- **ct_res 通用臂**:Some/None/Ok/Err 四臂 match 发射、`.or/.expect/is_some` 族、R-branch 全部就绪。
- **尾判定缺口**:ct_stmts 无「尾位」标志穿线(统一批前置)。✅ 已销(2026-10-28,R2b:`#tailpos` env 穿线,见 §4 R2b 条)

## 2. 目标

Option/Result 作用户形参/本地位/返回值三位置获得正确 C 表示;match 对三族生产者分派正确;不破坏 NULL 模型与通道 hR 存量。

## 3. 方案

### 方案 A:R 码复合化 `R<ok>[,<err>]`(如 `Rs`/`Ri,E:Fail`)

- 首字符保 'R',既有 `byte_at(code,0)==82` 前缀判定不破;`== "R"` 精确比对的 7 处(trans_ty×2/trans_expr×3/trans_stmt×2)改前缀判定。
- ct_ctype 复合臂:`Rs`/`Rv` → char*(Str 载荷 NULL 模型对齐);其余 → `ct_res`。
- 绑定点(#elem 同族)写入复合码,match 分派/构造臂按复合码选模型。
- **优点**:一处码形,全链自洽。**缺点**:触碰全部 R 消费面(计数 7+10),回归面最大。

### 方案 B:侧表+节点解析(推荐分期首步)

- 不改码形;形参/注解 let/返回值三位置在**绑定创建点**从 AST 注解节点解析(方案 A 的 ct_ctype 复合逻辑以 fn 形态落 `ct_ctype_of_node(nt)`),#cerr 同族侧条目携带。
- Some/None 构造位增加**期望型线程**(LET 注解/#cret 标记):Option[Str] 期望 → 裸 char*/0;其余 → ct_res。
- **优点**:增量、可分期(Str? 族先行),不碰既有 R 消费面。**缺点**:码仍不自带信息,跨 fn 传递仍靠 decl 查询。

### 方案 C:ct_res 统一+NULL 模型适配器

- read_file 族包 ct_res;NULL 模型退役。**否决**:read_file 为 extern 直返 char*,包装=每次分配+并发层 ABI 牵连。

## 4. 推荐

**B 先行,分两批**:
- **批 R1(Str? 族)**:✅ 已落地(ca79e556,2026-10-19)。实现与设计有二偏离:①翻面点=ct_ty_code(`Str?` 解析为 **Optional 节点**非 Named-Option,检查先于 Named 闸);②构造线程以 `ct_optstr_unr` 适配器替代(#cerr 同族侧标记免穿线)。
- **批 R2(其余载荷)**:✅ **R2a 零码改完成(2026-10-20 验证)**——R1 的 Optional 臂(`非 Str → "R"`)使 `I32?`/`Result[T,E]` 取码 "R",而 `ct_ctype("R")` 本就返回 `ct_res`(塌缩真身=Optional 节点无臂落 "i",非 ctype)。形参/注解 let/返回/直构实参/`.or`/`?` 传播全链双臂逐字一致;fx 语料转正=fx_opti32/fx_opti32b(smoke 名单随批)。**余 R2b=尾判定统一**(语句位 match 非块值臂提前返回误用面;需 ct_stmts 尾标志穿线,值臂落暂存形),设计裁决项:尾/语句位两制是否保留(尾值臂 return 位=fx_optstr 依赖,语句位=解释臂语义)。
  - ✅ **R2b 落地(2026-10-28)**——`#tailpos` env 穿线(spec 前置缺口 :34 销):fn/闭包/子块尾位 match 的 OK/ERR 非块值臂转 return 位(interp 尾值语义对齐;修复前 emit 落空 UB,fx_restail 双臂实证);SOME/NONE return 位与中位「丢弃+写法纪律」两制**保留**(裁决=勘定收束③,fx_optstr 金源不动);死码 ct_match_value/尾分派 If 值形撤(不可达实证);解析层尾分类契约显式化(形状包裹否决:sem 走查器裸 tag 直读静默弱化面)。fx_restail 入 smoke。局限登记见 COVERAGE 同日批(嵌套中位误染/If-Try 尾位同族/值位块 match 未勘定)。
- 方案 A 作为码形清整理想态,**非前置**。

## 5. 关联

- IP-01 W2 错误链(#cerr/ct_err_elem_of/变体名表已就位,本设计直接复用)。
- IP-23 23a(json 数值模型,同"表示分裂"病根家族,设计轮另立)。
- 在册:match R 臂尾判定统一批(本设计 §1.2-5)。
