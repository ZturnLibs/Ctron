<!-- 英文译件:手维护;中文正典 = docs/spec/ 同名文件(经 tools/sync_site_spec.sh 同步至同名 .zh.md) -->
<!-- 译件滞后于正典修订时,以中文正典为准 -->

# §1 Lexical & Grammar

## 1.1 Source Files

- Source files must be UTF-8 encoded; extension `.ct`.
- A source file is a **module** (§2.1); declaration order within a file is irrelevant (§2.6).
- Whitespace: space, Tab, newline. **Indentation carries no meaning** (P8/tokenizer friendly).
- Newlines are uniformly handled as `\n` (`\r\n` is normalized).

## 1.2 Comments

- Line comments `//` run to end of line; **block comments do not exist** (keeps the tokenizer simple and grep friendly).
- Doc comments `///` must immediately precede the declaration they document; code blocks inside doc comments participate in doc-test compilation and execution (§10.4).

## 1.3 Identifiers & Keywords

- Identifiers: `[A-Za-z_][A-Za-z0-9_]*` (Unicode identifiers reserved). Externally visible names must not begin with an underscore (a leading `_` is for internal/placeholder use only).
- Naming conventions (enforced as lint, not syntax errors):
  - Types/enums/variants/constructors: `PascalCase`
  - Functions/bindings/fields: `snake_case`
  - Constants/statics: `SCREAMING_CASE`
  - Package names: all-lowercase words
- **Keywords** (cannot be used as identifiers; the single authoritative list):

```
fn let var const static comptime
if else match while for in return
struct class enum trait impl
own scope test use pub extern
prop true false void self
```

- **Reserved operator words** (cannot be used as identifiers): `or` (infix default value, §4.4).
- **Reserved words** (currently syntax errors, reserved for evolution): `do async await interface module` (as of v0.7, `break continue` are promoted to keywords, §4.2).
- `as` is **not a keyword** — explicit numeric conversion is a prelude method on numeric types, `x.as[U64]()` (§3.6). `arena`, `Box`, `List`, `String`, `Channel`, `Mutex`, `Arena`, `Option`, `Result`, etc. are prelude types/bindings, not keywords.
- Forbidden punctuation (syntax errors): `;` `::`. `!` serves only as unary not; `&` appears only in types; `?` only as a suffix.

## 1.4 Literals

- **Integer literals**: decimal; `0x` hexadecimal, `0o` octal, `0b` binary; may contain `_` separators (`1_000_000`).
  - Without a suffix the default is `I32`; in contexts where the expected type is clear, it **adapts** to the expected integer type (§3.7).
  - Suffixes: `i8 i16 i32 i64 isize u8 u16 u32 u64 usize f32 f64` (e.g. `255u8`).
- **Floating-point literals**: decimal, may carry an exponent; the default without a suffix is `F64`; suffixes `f32`/`f64`.
- **Booleans**: `true` / `false`.
- **No character literals** (there is no `char` type; code point iteration returns integers via read-only stdlib APIs).
- **String literals**: double-quoted; escapes `\n \t \r \\ \" \0 \{ \u{HEX}`; the literal's type is `Str` (an immutable borrow, §3.3), stored in static storage (available in every profile).
  - **Interpolation**: `{` introduces an interpolation expression, which may contain identifiers, field/property/method chains, and indexing (`{clock.now()}`, `{xs[0]}`); a literal `{` must be written `\{`. Interpolated strings are syntactic sugar, equivalent to a `fmt` call over concatenated fragments (§4.6).

## 1.5 Operators & Punctuation

```
+  -  *  /  %        arithmetic
+% -% *= /= %= +=    wrapping add/sub; compound assignment
== != <  >  <= >=    comparison
=                    assignment (statement only, §4.2)
&&                   logical and (short-circuit)
||                   logical or (short-circuit, v0.7; §4.4)
-  !                  unary: negation / logical not (Bool only; §4.3 level 7, §4.4)
or                   infix default value (Option/Result, §5.2); not logical or
.. ..=               range (half-open / closed)
..                   slice type / elision (see grammar)
-> =>                return type / match arm
?                    Result/Option propagation suffix (§5.3)
.                    path/field/method/tuple index (.0 .1)
, : ;                forbidden
[ ] ( ) { }          generic args, grouping/tuple/params, blocks
&                    types only: shared read-only references (&T / &Trait)
#[@ ] @derive(...)   annotations / derive (§8.3)
|                    delimits closure parameters
_                    wildcard
```

## 1.6 Newline Termination Rules (Statement Delimiting)

Ctron has no semicolons. **A newline is the terminator of statements/fields/variants/match arms**, unless any of the following holds:

1. The line's final token belongs to the continuation set (the line's semantics are incomplete):

```
,  =  ->  =>  &&  ||  or  ..  ..=  +  -  *  /  %  +%  -%  ==  !=  <  >  <=  >=  (  [  {  |
```(`?` is not in the continuation set: it is a suffix and always terminates a statement)

2. **The next line starts with `.` or a binary operator** (supporting the "leading-dot" layout of chained calls:

```c
let y = xs
    .filter(|x| x > 0)
    .map(|x| x * 2)
```

A leading `.` is therefore **always** a continuation of the previous line's expression, never the start of a new statement; conversely, a trailing `.` is not in the continuation set, so trailing-dot chaining is illegal (leading-dot style is mandated, and the formatter emits the unique form).

- `else` must be on the same line as `}`: `} else {`. The cross-line form (a newline after `}`, then `else`) is rejected by the parser (E1001) — the lexer does not suppress that newline.
- After the last expression in a block (the block value), `}` may follow directly without a newline.

## 1.7 Full Grammar (EBNF)

Notation: `{ X }` repetition, `[ X ]` optionality, `|` alternation, `NEWLINE` a newline. `NL+` at the right end of a production means "newline-separated repetition".

```ebnf
(* ---------- top level ---------- *)
File        = { TopDecl } ;
TopDecl     = UseDecl | StructDecl | ClassDecl | EnumDecl | TraitDecl | ImplDecl
            | FnDecl | ConstDecl | StaticDecl | TestDecl ;
UseDecl     = "use" Path [ "{" Path { "," Path } [ "," ] "}" ] NEWLINE ;
Path        = IDENT { "." IDENT } ;

(* ---------- type declarations ---------- *)
StructDecl  = { DeclAttr } "struct" IDENT [ TypeParams ] "{" NEWLINE* { Field NEWLINE+ } "}" ;
Field       = Visibility [ "let" | "var" ] IDENT ":" Type ;   (* let may be omitted; let/omitted = immutable, var = mutable *)
ClassDecl   = { DeclAttr } "class" IDENT [ TypeParams ] "{" NEWLINE* { ClassItem NEWLINE+ } "}" ;
ClassItem   = Field | Method | PropImpl ;
EnumDecl    = { DeclAttr } "enum" IDENT [ TypeParams ] "{"
              NEWLINE* { Variant NEWLINE+ } "}" ;
Variant     = IDENT [ "(" [ Type { "," Type } ] ")"
                    | "{" Field { ( "," | NEWLINE+ ) Field } [ "," ] "}" ] ;   (* variant fields: separated by commas or newlines *)
TypeParams  = "[" TypeParam { "," TypeParam } "]" ;
TypeParam   = IDENT [ ":" Bound ] | "comptime" IDENT ":" Type ;
Bound       = Path { "+" Path } ;
Visibility  = "pub" | "pub" "(" "pkg" ")" ;

(* ---------- traits and impls ---------- *)
TraitDecl   = { DeclAttr } "trait" IDENT [ TypeParams ] [ ":" Bound ]
              "{" NEWLINE* { TraitItem NEWLINE+ } "}" ;      (* Bound = supertrait *)
TraitItem   = Method | PropSig | PropImpl | ConstDecl ;
Method      = { DeclAttr } Visibility "fn" IDENT [ TypeParams ] "(" ParamList ")" [ "->" Type ] Block ;
PropSig     = Visibility "prop" IDENT ":" Type ;
PropImpl    = Visibility "prop" IDENT ":" Type Block ;
ImplDecl    = "impl" [ TypeParams ] Path [ TypeArgs ] "for" Type
              "{" NEWLINE* { (Method | PropImpl) NEWLINE+ } "}" ;

(* ---------- functions and tests ---------- *)
FnDecl      = { DeclAttr } [ "pub" ] [ "comptime" ] [ "extern" STRING_LIT ] "fn" IDENT
              [ TypeParams ] "(" ParamList ")" [ "->" Type ] [ Block ] ;   (* extern declarations omit Block, §9.6 *)
ParamList   = [ Param { "," Param } [ "," ] ] ;
Param       = Receiver | [ "var" ] IDENT ":" Type ;
Receiver    = "&" "self" | "var" "self" ;
ConstDecl   = "const" IDENT ":" Type "=" Expr NEWLINE ;
StaticDecl  = "static" "let" IDENT ":" Type "=" Expr NEWLINE ;
TestDecl    = "test" STRING_LIT Block ;

(* ---------- attributes ---------- *)
DeclAttr    = Attribute | DeriveAttr ;            (* modifies the declaration that immediately follows *)
Attribute   = "#[" IDENT [ "(" AttrArgs ")" ] "]" ;
AttrArgs    = Expr | IDENT { "," (Expr | IDENT) } ;
DeriveAttr  = "@derive" "(" Path { "," Path } ")" ;

(* ---------- statements ---------- *)
Block       = "{" NEWLINE* { (Stmt | Expr) NEWLINE+ } [ Expr NEWLINE* ] "}" ;
Stmt        = LetStmt | VarStmt | ReturnStmt | ForStmt | WhileStmt | AssignStmt | ExprStmt ;
LetStmt     = "let" Pattern [ ":" Type ] "=" Expr ;
VarStmt     = "var" Pattern [ ":" Type ] "=" Expr ;
ReturnStmt  = "return" [ Expr ] ;
ForStmt     = "for" Pattern "in" Expr Block ;
WhileStmt   = "while" Expr Block ;
AssignStmt  = PostfixExpr AssignOp Expr ;
AssignOp    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

(* ---------- patterns ---------- *)
Pattern     = IDENT | "_" | LiteralPattern | TuplePattern | AggPattern ;
LiteralPattern = INT_LIT | FLOAT_LIT | STRING_LIT | "true" | "false" ;
TuplePattern = "(" [ Pattern { "," Pattern } ] ")" ;
AggPattern  = PathPattern [ "(" [ Pattern { "," Pattern } ] ")"
                        | "{" FieldPattern { "," FieldPattern } "}" ] ;
PathPattern = IDENT { "." IDENT } ;   (* enum variants / named types *)
FieldPattern= IDENT | IDENT ":" Pattern ;

(* ---------- expressions (ascending precedence; see §4.3 for details) ---------- *)
Expr        = LogicOrOr ;
LogicOrOr   = LogicOr { "||" LogicOr } ;    (* v0.7: level 0; a || in starting position begins a zero-parameter closure, see §4.7 role separation *)
LogicOr     = LogicAnd { "or" LogicAnd } ;
LogicAnd    = Compare { "&&" Compare } ;
Compare     = Range [ ( "==" | "!=" | "<" | ">" | "<=" | ">=" ) Range ] ;   (* not chainable *)
Range       = Additive [ ( ".." | "..=" ) Additive ] ;
Additive    = Multiplicative { ( "+" | "-" | "+%" | "-%" ) Multiplicative } ;
Multiplicative = Unary { ( "*" | "/" | "%" ) Unary } ;
Unary       = ( "-" | "!" ) Unary | Postfix ;
Postfix     = Primary { Call | Index | Member | TypeArgs | Try } ;
Call        = "(" [ Expr { "," Expr } [ "," ] ] ")" ;
Index       = "[" Expr "]" ;
Member      = "." ( IDENT | INT_LIT ) ;            (* INT_LIT: tuple .0 .1 *)
TypeArgs    = "[" Type { "," Type } "]" ;
Try         = "?" ;

Primary     = INT_LIT | FLOAT_LIT | STRING_LIT | "true" | "false" | "void" | IDENT
            | "(" [ Expr { "," Expr } ] ")"             (* grouping / tuple *)
            | ArrayLit | StructLit | IfExpr | MatchExpr
            | Closure | ScopeExpr | OwnExpr | Block ;
ArrayLit    = "[" [ Expr { "," Expr } [ "," ] ] "]" ;
StructLit   = Path [ TypeArgs ] "{" FieldInit { "," FieldInit } [ "," ] "}" ;
FieldInit   = IDENT [ ":" Expr ] ;                   (* omitted = shorthand for the same-named field *)
IfExpr      = "if" Expr Block ( "else" ( IfExpr | Block ) ) ;   (* else is required when used as a value *)
MatchExpr   = "match" Expr "{" NEWLINE* { MatchArm NEWLINE+ } "}" ;
MatchArm    = Pattern "=>" Expr ;
Closure     = "|" [ ClosureParam { "," ClosureParam } ] "|" [ "->" Type ] Expr ;
ClosureParam= [ "var" ] IDENT [ ":" Type ] ;
ScopeExpr   = "scope" ClosureBlock ;
ClosureBlock= "{" "|" IDENT "|" NEWLINE* { (Stmt | Expr) NEWLINE+ } [ Expr NEWLINE* ] "}" ;
OwnExpr     = "own" "(" IDENT ")" Block ;

(* ---------- types ---------- *)
Type        = "&" Type                  (* shared read-only reference / trait object *)
            | Type "[" "]"              (* mutable slice view T[] (§3.1; &T[] is the read-only view) *)
            | Type "[" Expr "]"         (* fixed-size array T[N]; N is a comptime expression *)
            | Type "?"                  (* Option sugar *)
            | "(" [ Type { "," Type } ] ")"   (* tuple / unit "()")
            | "fn" "(" [ Type { "," Type } ] ")" [ "->" Type ]   (* function type: parameter/return positions only *)
            | Path [ TypeArgs ]         (* named/generic type *)
            | "self" ;                  (* inside impl: refers to the implementing type *)
```

## 1.8 Grammar Ambiguity Rulings

- **`IDENT {` in expression position**: always a construction literal (of a named type). Bare blocks can only appear in keyword-introduced positions (`fn`/`if`/`else`/`while`/`for`/`own`/`scope`/`match` arms, closure bodies), so the two never conflict.
- **`IDENT [ ... ]` in expression position**: if `[...]` is **immediately followed** by `(` or `{` → generic instantiation (e.g. `Channel[I32](4)`, `Box[Point](p)`); otherwise parsed as indexing, and **when the index content is not a legal single expression (e.g. it contains a top-level comma) it falls back to type arguments** (covering `Simd[F32, 4].splat(v)`). Consequently, "indexing an array of closures and then calling" requires parentheses: `(xs[i])(arg)` — the parser can decide without type information.
- Generics use `[]` rather than `<>` (eliminating the `a < b > c` ambiguity; a normative ruling, from a test pinned ruling).
- The function type `fn(...) -> T` appears only in type positions; `fn` in expression position is an illegal Primary, so there is no ambiguity.
- **`[` in type position disambiguation (P1-B ruling)**: empty `[]` → slice; content is a **single integer literal** → fixed-size array `Type [ Expr ]`; anything else → generic type arguments. Residual ambiguity: `T[SIZE]` (where SIZE is a comptime constant identifier) parses as generic — fixed-size arrays use literal dimensions, or are used via the comptime-monomorphized `Arr[T, N]` form.

## 1.9 Correspondence with the Test Suite

Executable samples for lexical/grammar features: `tests/01_basics.ct` (literals/operations/loops), `tests/04_generics_comptime.ct` (generic and comptime syntax).
