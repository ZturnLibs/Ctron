# §1 词法与语法

## 1.1 源文件

- 源文件必须为 UTF-8 编码;扩展名 `.ct`。
- 源文件是一个**模块**(§2.1);文件内声明顺序无关(§2.6)。
- 空白符:空格、Tab、换行。**缩进无语义**(P8/tokenizer 友好)。
- 换行统一按 `\n` 处理(`\r\n` 归一化)。

## 1.2 注释

- 行注释 `//` 到行尾;**块注释不存在**(tokenizer 简单、grep 友好)。
- 文档注释 `///`,必须紧邻被文档声明之前;文档注释中的代码块参与 doc-test 编译与运行(§10.4)。

## 1.3 标识符与关键字

- 标识符:`[A-Za-z_][A-Za-z0-9_]*`(Unicode 标识符预留)。禁止以下划线开头的外部可见名(`_` 开头仅用于内部/占位)。
- 命名约定(强制 lint,非语法错误):
  - 类型/枚举/变体/构造:`PascalCase`
  - 函数/绑定/字段:`snake_case`
  - 常量/静态:`SCREAMING_CASE`
  - 包名:全小写单词
- **关键字**(不能作标识符,唯一权威清单):

```
fn let var const static comptime
if else match while for in return
struct class enum trait impl
own scope test use pub extern
prop true false void self
```

- **保留运算符字**(不可作标识符):`or`(取默认中缀,§4.4)。
- **预留字**(当前为语法错误,为演进保留):`do async await interface module`(v0.7 起 `break continue` 转正为关键字,§4.2)。
- `as` **不是关键字**——数值显式转换是数值类型的前奏方法 `x.as[U64]()`(§3.6)。`arena`、`Box`、`List`、`String`、`Channel`、`Mutex`、`Arena`、`Option`、`Result` 等是前奏类型/绑定,不是关键字。
- 禁用的标点(语法错误):`;` `::`。`!` 仅作一元非;`&` 仅出现在类型中;`?` 仅作后缀。

## 1.4 字面量

- **整数字面量**:十进制;`0x` 十六进制、`0o` 八进制、`0b` 二进制;可含 `_` 分隔(`1_000_000`)。
  - 无后缀时默认 `I32`;在期望类型明确的上下文中**自适应**到期望的整数类型(§3.7)。
  - 后缀:`i8 i16 i32 i64 isize u8 u16 u32 u64 usize f32 f64`(如 `255u8`)。
- **浮点字面量**:十进制,可含指数;无后缀默认 `F64`;后缀 `f32`/`f64`。
- **布尔**:`true` / `false`。
- **无字符字面量**(`char` 类型不存在;码点迭代经 stdlib 只读 API 返回整数)。
- **字符串字面量**:双引号;转义 `\n \t \r \\ \" \0 \{ \u{HEX}`;字面量类型为 `Str`(不可变借用,§3.3),存放于静态存储(任何档位可用)。
  - **插值**:`{` 引入插值表达式,可含标识符、字段/属性/方法链与索引(`{clock.now()}`、`{xs[0]}`);字面 `{` 必须写 `\{`。插值串是语法糖,等价于对片段拼接的 `fmt` 调用(§4.6)。

## 1.5 运算符与标点

```
+  -  *  /  %        算术
+% -% *= /= %= +=    回绕加/减;复合赋值
== != <  >  <= >=    比较
=                    赋值(仅语句,§4.2)
&&                   逻辑与(short-circuit)
||                   逻辑或(short-circuit,v0.7;§4.4)
or                   中缀取默认(Option/Result,§5.2);非逻辑或
.. ..=               range(左闭右开/双闭)
..                   切片类型/省略(见语法)
-> =>                返回类型 / match 分支
?                    Result/Option 传播后缀(§5.3)
.                    路径/字段/方法/元组索引(.0 .1)
, : ;(禁用)
[ ] ( ) { }          泛型实参、分组/元组/参数、块
&                    仅类型:共享只读引用(&T / &Trait)
#[@ ] @derive(...)   注解 / derive(§8.3)
|                    闭包参数界定
_                    通配
```

## 1.6 换行终止规则(语句定界)

Ctron 无分号。**换行是语句/字段/变体/match 臂的终止符**,除非满足以下任一条件:

1. 行尾 token 属于延续集(该行语义未完成):

```
,  =  ->  =>  &&  ||  or  ..  ..=  +  -  *  /  %  +%  -%  ==  !=  <  >  <=  >=  (  [  {  |
```(`?` 不在延续集:它是后缀,总终结语句)

2. **下一行以 `.` 或二元运算符开头**(支持链式调用的"首点排版":

```c
let y = xs
    .filter(|x| x > 0)
    .map(|x| x * 2)
```

行首 `.` 因此**永远**是前一行表达式的继续,不是新语句的开始;反之,行尾 `.` 不在延续集中,尾点式链式写法非法(统一用首点式,formatter 输出唯一形态)。

- `else` 必须与 `}` 同行:`} else {`。跨行(`}` 后换行再 `else`)由解析器拒绝(E1001)——词法层不抑制该换行。
- 块内最后一个表达式(块值)后可无换行直接 `}`。

## 1.7 完整语法(EBNF)

记法:`{ X }` 重复、`[ X ]` 可选、`|` 选择、`NEWLINE` 换行。产生式右端的 `NL+` 表示"以换行分隔的重复"。

```ebnf
(* ---------- 顶层 ---------- *)
File        = { TopDecl } ;
TopDecl     = UseDecl | StructDecl | ClassDecl | EnumDecl | TraitDecl | ImplDecl
            | FnDecl | ConstDecl | StaticDecl | TestDecl ;
UseDecl     = "use" Path [ "{" Path { "," Path } [ "," ] "}" ] NEWLINE ;
Path        = IDENT { "." IDENT } ;

(* ---------- 类型声明 ---------- *)
StructDecl  = { DeclAttr } "struct" IDENT [ TypeParams ] "{" NEWLINE* { Field NEWLINE+ } "}" ;
Field       = Visibility [ "let" | "var" ] IDENT ":" Type ;   (* let 可省略;let/省略 = 不可变,var = 可变 *)
ClassDecl   = { DeclAttr } "class" IDENT [ TypeParams ] "{" NEWLINE* { ClassItem NEWLINE+ } "}" ;
ClassItem   = Field | Method | PropImpl ;
EnumDecl    = { DeclAttr } "enum" IDENT [ TypeParams ] "{"
              NEWLINE* { Variant NEWLINE+ } "}" ;
Variant     = IDENT [ "(" [ Type { "," Type } ] ")"
                    | "{" Field { ( "," | NEWLINE+ ) Field } [ "," ] "}" ] ;   (* 变体字段:逗号或换行分隔 *)
TypeParams  = "[" TypeParam { "," TypeParam } "]" ;
TypeParam   = IDENT [ ":" Bound ] | "comptime" IDENT ":" Type ;
Bound       = Path { "+" Path } ;
Visibility  = "pub" | "pub" "(" "pkg" ")" ;

(* ---------- trait 与 impl ---------- *)
TraitDecl   = { DeclAttr } "trait" IDENT [ TypeParams ] [ ":" Bound ]
              "{" NEWLINE* { TraitItem NEWLINE+ } "}" ;      (* Bound = 超 trait *)
TraitItem   = Method | PropSig | PropImpl | ConstDecl ;
Method      = { DeclAttr } Visibility "fn" IDENT [ TypeParams ] "(" ParamList ")" [ "->" Type ] Block ;
PropSig     = Visibility "prop" IDENT ":" Type ;
PropImpl    = Visibility "prop" IDENT ":" Type Block ;
ImplDecl    = "impl" [ TypeParams ] Path [ TypeArgs ] "for" Type
              "{" NEWLINE* { (Method | PropImpl) NEWLINE+ } "}" ;

(* ---------- 函数与测试 ---------- *)
FnDecl      = { DeclAttr } [ "pub" ] [ "comptime" ] [ "extern" STRING_LIT ] "fn" IDENT
              [ TypeParams ] "(" ParamList ")" [ "->" Type ] [ Block ] ;   (* extern 声明省略 Block,§9.6 *)
ParamList   = [ Param { "," Param } [ "," ] ] ;
Param       = Receiver | [ "var" ] IDENT ":" Type ;
Receiver    = "&" "self" | "var" "self" ;
ConstDecl   = "const" IDENT ":" Type "=" Expr NEWLINE ;
StaticDecl  = "static" "let" IDENT ":" Type "=" Expr NEWLINE ;
TestDecl    = "test" STRING_LIT Block ;

(* ---------- 属性 ---------- *)
DeclAttr    = Attribute | DeriveAttr ;            (* 修饰紧随其后的声明 *)
Attribute   = "#[" IDENT [ "(" AttrArgs ")" ] "]" ;
AttrArgs    = Expr | IDENT { "," (Expr | IDENT) } ;
DeriveAttr  = "@derive" "(" Path { "," Path } ")" ;

(* ---------- 语句 ---------- *)
Block       = "{" NEWLINE* { (Stmt | Expr) NEWLINE+ } [ Expr NEWLINE* ] "}" ;
Stmt        = LetStmt | VarStmt | ReturnStmt | ForStmt | WhileStmt | AssignStmt | ExprStmt ;
LetStmt     = "let" Pattern [ ":" Type ] "=" Expr ;
VarStmt     = "var" Pattern [ ":" Type ] "=" Expr ;
ReturnStmt  = "return" [ Expr ] ;
ForStmt     = "for" Pattern "in" Expr Block ;
WhileStmt   = "while" Expr Block ;
AssignStmt  = PostfixExpr AssignOp Expr ;
AssignOp    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

(* ---------- 模式 ---------- *)
Pattern     = IDENT | "_" | LiteralPattern | TuplePattern | AggPattern ;
LiteralPattern = INT_LIT | FLOAT_LIT | STRING_LIT | "true" | "false" ;
TuplePattern = "(" [ Pattern { "," Pattern } ] ")" ;
AggPattern  = PathPattern [ "(" [ Pattern { "," Pattern } ] ")"
                        | "{" FieldPattern { "," FieldPattern } "}" ] ;
PathPattern = IDENT { "." IDENT } ;   (* 枚举变体 / 具名类型 *)
FieldPattern= IDENT | IDENT ":" Pattern ;

(* ---------- 表达式(按优先级升序,详见 §4.3) ---------- *)
Expr        = LogicOrOr ;
LogicOrOr   = LogicOr { "||" LogicOr } ;    (* v0.7:第 0 层;起始位置 || 为零参闭包起始,见 §4.7 角色分离 *)
LogicOr     = LogicAnd { "or" LogicAnd } ;
LogicAnd    = Compare { "&&" Compare } ;
Compare     = Range [ ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) Range ] ;   (* 不可链 *)
Range       = Additive [ ( ".." | "..=" ) Additive ] ;
Additive    = Multiplicative { ( "+" | "-" | "+%" | "-%" ) Multiplicative } ;
Multiplicative = Unary { ( "*" | "/" | "%" ) Unary } ;
Unary       = ( "-" | "!" ) Unary | Postfix ;
Postfix     = Primary { Call | Index | Member | TypeArgs | Try } ;
Call        = "(" [ Expr { "," Expr } [ "," ] ] ")" ;
Index       = "[" Expr "]" ;
Member      = "." ( IDENT | INT_LIT ) ;            (* INT_LIT: 元组 .0 .1 *)
TypeArgs    = "[" Type { "," Type } "]" ;
Try         = "?" ;

Primary     = INT_LIT | FLOAT_LIT | STRING_LIT | "true" | "false" | "void" | IDENT
            | "(" [ Expr { "," Expr } ] ")"             (* 分组 / 元组 *)
            | ArrayLit | StructLit | IfExpr | MatchExpr
            | Closure | ScopeExpr | OwnExpr | Block ;
ArrayLit    = "[" [ Expr { "," Expr } [ "," ] ] "]" ;
StructLit   = Path [ TypeArgs ] "{" FieldInit { "," FieldInit } [ "," ] "}" ;
FieldInit   = IDENT [ ":" Expr ] ;                   (* 缺省 = 同名字段简写 *)
IfExpr      = "if" Expr Block ( "else" ( IfExpr | Block ) ) ;   (* 作为值时 else 必需 *)
MatchExpr   = "match" Expr "{" NEWLINE* { MatchArm NEWLINE+ } "}" ;
MatchArm    = Pattern "=>" Expr ;
Closure     = "|" [ ClosureParam { "," ClosureParam } ] "|" [ "->" Type ] Expr ;
ClosureParam= [ "var" ] IDENT [ ":" Type ] ;
ScopeExpr   = "scope" ClosureBlock ;
ClosureBlock= "{" "|" IDENT "|" NEWLINE* { (Stmt | Expr) NEWLINE+ } [ Expr NEWLINE* ] "}" ;
OwnExpr     = "own" "(" IDENT ")" Block ;

(* ---------- 类型 ---------- *)
Type        = "&" Type                  (* 共享只读引用 / trait 对象 *)
            | Type "[" "]"              (* 可变切片视图 T[](§3.1;&T[] 为只读视图) *)
            | Type "[" Expr "]"         (* 定长数组 T[N],N 为 comptime 表达式 *)
            | Type "?"                  (* Option 糖 *)
            | "(" [ Type { "," Type } ] ")"   (* 元组 / 单元 "()")
            | "fn" "(" [ Type { "," Type } ] ")" [ "->" Type ]   (* 函数类型:仅参数/返回位 *)
            | Path [ TypeArgs ]         (* 命名/泛型类型 *)
            | "self" ;                  (* impl 内指代实现类型 *)
```

## 1.8 语法歧义裁决

- **表达式位置的 `IDENT {`**:恒为(具名类型的)构造字面量。裸块只能出现在关键字引导的位置(`fn`/`if`/`else`/`while`/`for`/`own`/`scope`/`match` 臂、闭包体),二者不冲突。
- **表达式位置的 `IDENT [ ... ]`**:若 `[...]` **紧跟** `(` 或 `{` → 泛型实例化(如 `Channel[I32](4)`、`Box[Point](p)`);否则按索引解析,**索引内容不是合法单表达式时(如含顶层逗号)回退为类型实参**(覆盖 `Simd[F32, 4].splat(v)`)。由此,对"数组元素为闭包再调用"必须加括号:`(xs[i])(arg)`——解析器无需类型信息即可判定。
- 泛型用 `[]` 而非 `<>`(消除 `a < b > c` 歧义;规范性裁决,来自测试钉子)。
- 函数类型 `fn(...) -> T` 仅出现在类型位置;表达式位置 `fn` 是非法 Primary,无歧义。
- **类型位置的 `[` 消歧(P1-B 裁决)**:空 `[]` → 切片;内容为**单个整数字面量** → 定长数组 `Type [ Expr ]`;其余 → 泛型类型实参。残余歧义:`T[SIZE]`(SIZE 为 comptime 常量标识符)按泛型解析——定长数组用字面量维度,或经 comptime 单态化 `Arr[T, N]` 形态使用。

## 1.9 与测试集的对应

词法/语法的可执行样例:`tests/01_basics.ct`(字面量/运算/循环)、`tests/04_generics_comptime.ct`(泛型与 comptime 语法)。
