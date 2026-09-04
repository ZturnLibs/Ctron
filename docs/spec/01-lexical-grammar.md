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
- **关键字**(不能作标识符):

```
fn let var const static comptime
if else match while for in return
struct class enum trait impl
own scope test use pub
prop as true false void self
and? (无) — 逻辑与为 &&
```

  完整清单:`fn let var const static comptime if else match while for in return struct class enum trait impl own scope test use pub prop true false void self`。`or` 不是关键字而是**中缀运算符**(§4.4);`arena`、`Box`、`List`、`String`、`Channel`、`Mutex`、`Arena`、`Option`、`Result` 等是前奏类型/绑定,不是关键字。
- 保留未用(语法错误):`;` `::` `!`(仅作一元非)`&`(仅出现在类型中)`?`(仅作后缀)。

## 1.4 字面量

- **整数字面量**:十进制;`0x` 十六进制、`0o` 八进制、`0b` 二进制;可含 `_` 分隔(`1_000_000`)。
  - 无后缀时默认 `I32`;在期望类型明确的上下文中**自适应**到期望的整数类型(§3.7)。
  - 后缀:`i8 i16 i32 i64 isize u8 u16 u32 u64 usize f32 f64`(如 `255u8`)。
- **浮点字面量**:十进制,可含指数;无后缀默认 `F64`;后缀 `f32`/`f64`。
- **布尔**:`true` / `false`。
- **无字符字面量**(`char` 类型不存在;码点迭代经 stdlib 只读 API 返回整数)。
- **字符串字面量**:双引号;转义 `\n \t \r \\ \" \0 \{ \u{HEX}`;字面量类型为 `Str`(不可变借用,§3.3),存放于静态存储(任何档位可用)。
  - **插值**:`{` 引入插值表达式,可含标识符、字段/方法链(`{clock.now()}`);字面 `{` 必须写 `\{`。插值串是语法糖,等价于对片段拼接的 `fmt` 调用(§4.6)。

## 1.5 运算符与标点

```
+  -  *  /  %        算术
+% -% *= /= %= +=    回绕加/减;复合赋值
== != <  >  <= >=    比较
=                    赋值(仅语句,§4.2)
&&                   逻辑与(short-circuit)
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

Ctron 无分号。**换行是语句/字段/变体/match 臂的终止符**,除非行尾 token 属于延续集:

```
,  =  ->  =>  &&  or  ..  ..=  +  -  *  /  %  +%  -%  ==  !=  <  >  <=  >=  (  [  {  |  ?  and.  — 即:行尾语义未完成
```

- `else` 必须与 `}` 同行:`} else {`。
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
StructDecl  = "struct" IDENT [ TypeParams ] "{" NEWLINE* { Field NEWLINE+ } "}" ;
Field       = [ "pub" ] [ "var" ] IDENT ":" Type ;
ClassDecl   = "class" IDENT [ TypeParams ] "{" NEWLINE* { ClassItem NEWLINE+ } "}" ;
ClassItem   = Field | Method ;
EnumDecl    = "enum" IDENT [ TypeParams ] "{"
              NEWLINE* { Variant NEWLINE+ } "}" ;
Variant     = IDENT [ "(" [ Type { "," Type } ] ")"
                    | "{" Field { NEWLINE+ Field } "}" ] ;
TypeParams  = "[" TypeParam { "," TypeParam } "]" ;
TypeParam   = IDENT [ ":" Bound ] | "comptime" IDENT ":" Type ;
Bound       = Path { "+" Path } ;

(* ---------- trait 与 impl ---------- *)
TraitDecl   = "trait" IDENT [ TypeParams ] "{" NEWLINE* { TraitItem NEWLINE+ } "}" ;
TraitItem   = Method | PropSig | PropImpl | ConstDecl ;
Method      = [ "pub" ] "fn" IDENT [ TypeParams ] "(" ParamList ")" [ "->" Type ] Block ;
PropSig     = [ "pub" ] "prop" IDENT ":" Type ;
PropImpl    = [ "pub" ] "prop" IDENT ":" Type Block ;
ImplDecl    = "impl" [ TypeParams ] Path [ TypeArgs ] "for" Type
              "{" NEWLINE* { (Method | PropImpl) NEWLINE+ } "}" ;

(* ---------- 函数与测试 ---------- *)
FnDecl      = { Attribute } [ "pub" ] [ "comptime" ] "fn" IDENT [ TypeParams ]
              "(" ParamList ")" [ "->" Type ] Block ;
ParamList   = [ Param { "," Param } [ "," ] ] ;
Param       = Receiver | [ "var" ] IDENT ":" Type ;
Receiver    = "&" "self" | "var" "self" ;
ConstDecl   = "const" IDENT ":" Type "=" Expr NEWLINE ;
StaticDecl  = "static" "let" IDENT ":" Type "=" Expr NEWLINE ;
TestDecl    = "test" STRING_LIT Block ;

(* ---------- 属性 ---------- *)
Attribute   = "#[" IDENT [ "(" AttrArgs ")" ] "]" ;
AttrArgs    = Expr | IDENT { "," (Expr | IDENT) } ;
DeriveAttr  = "@derive" "(" Path { "," Path } ")" ;   (* 必须紧邻类型声明 *)

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
PathPattern = IDENT { "." IDENT } | IDENT ;   (* 枚举变体 / 具名类型 *)
FieldPattern= IDENT | IDENT ":" Pattern ;

(* ---------- 表达式(按优先级升序,详见 §4.3) ---------- *)
Expr        = LogicOr ;
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
            | Type "[" "]"              (* 切片 T[] *)
            | Type "[" Expr "]"         (* 定长数组 T[N],N 为 comptime 表达式 *)
            | Type "?"                  (* Option 糖 *)
            | "(" [ Type { "," Type } ] ")"   (* 元组 / 单元 "()")
            | Path [ TypeArgs ]         (* 命名/泛型类型 *)
            | "self" ;                  (* impl 内指代实现类型 *)
```

## 1.8 语法歧义裁决

- `IDENT {` 后若首个 token 是 `IDENT :` → 结构体/类字面量;否则为块(仅前奏类型名可出现于此形式)。
- `IDENT [` 在**类型位置**是类型实参,在**表达式位置**是索引;类型位置由解析状态决定,无运行时歧义。
- 泛型用 `[]` 而非 `<>`(消除 `a < b > c` 歧义;规范性裁决,来自测试钉子)。

## 1.9 与测试集的对应

词法/语法的可执行样例:`tests/01_basics.ct`(字面量/运算/循环)、`tests/04_generics_comptime.ct`(泛型与 comptime 语法)。
