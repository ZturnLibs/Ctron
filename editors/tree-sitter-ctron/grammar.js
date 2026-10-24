/**
 * tree-sitter-ctron — Ctron 语言的 tree-sitter 语法(highlight-grade)。
 *
 * 权威依据:docs/spec/01-lexical-grammar.md(§1.3 关键字、§1.4 字面量、
 * §1.5 运算符、§1.7 EBNF)。定位是**编辑器高亮**而非编译器前端:
 * - §1.6 的换行终止规则(延续集/首点式)是上下文敏感的,这里把换行当普通空白,
 *   语句靠运算符/结构自定界 —— 足够高亮,不做编译级判定;
 * - `[...]` 的三态(切片/定长数组/泛型实参,§1.8)在类型位全部接受,不做消歧裁决。
 */

const SUFFIX = /(?:i8|i16|i32|i64|isize|u8|u16|u32|u64|usize|f32|f64)?/.source;
const EXP = /(?:[eE][+-]?[0-9][0-9_]*)?/.source;
const FLOAT = new RegExp(`[0-9][0-9_]*(?:\\.[0-9][0-9_]*)?${EXP}${SUFFIX}`);
const INT = new RegExp(
  `(?:0x[0-9a-fA-F][0-9a-fA-F_]*|0o[0-7][0-7_]*|0b[01][01_]*|[0-9][0-9_]*)${SUFFIX}`);

module.exports = grammar({
  name: 'ctron',

  word: $ => $.identifier,

  extras: $ => [
    /[\s\uFEFF]/,
    $.line_comment,
  ],

  // §1.8 的 [ 三态(切片/定长/泛型)与闭合 | 零参闭包是真歧义,交 GLR
  conflicts: $ => [
    [$.path_type],
    [$.path],
    [$.return_stmt],
    [$._name, $._primary],
    [$._name, $._pattern],
    [$._type, $.self],
    [$.labeled_arg, $._expr],
    [$.subscript_expression, $._primary],
    [$.tuple_pattern, $.aggregate_pattern],
    [$.parenthesized_expression, $.tuple_type],
    [$.closure, $.scope_expression],
    [$.tag_attr],
    [$.decorator, $._primary],
  ],


  rules: {
    source_file: $ => repeat($._top_decl),

    // ── 顶层声明(§1.7 TopDecl)──
    _top_decl: $ => choice(
      $.use_decl,
      $.view_decl,
      $.style_decl,
      $.struct_decl,
      $.class_decl,
      $.enum_decl,
      $.trait_decl,
      $.impl_decl,
      $.function_decl,
      $.const_decl,
      $.static_decl,
      $.test_decl,
    ),

    use_decl: $ => seq(
      'use',
      // 独立规则:避免与 path 的 prec/冲突纠缠(3 段路径 + 花括号导入需稳定成活)
      $._use_mod,
      optional(seq('as', $.identifier)),
      optional(choice(
        seq('{', commaSep1($._use_item), optional(','), '}'),
        // '.{' 单 token:词法层与 path 的 '.' 消歧
        seq('.{', commaSep1($._use_item), optional(','), '}'),
      )),
    ),

    _use_mod: $ => seq($.identifier, repeat(seq('.', $.identifier))),

    // `x as y` 别名:as 非关键字(§1.3),按 identifier 匹配避免全局保留
    _use_item: $ => seq($.path, optional(seq($.identifier, $.path))),

    struct_decl: $ => seq(
      repeat($.decorator), optional($._visibility),
      'struct', field('name', $.identifier), optional($.type_params),
      '{', repeat(choice($._member, ',')), '}',
    ),

    class_decl: $ => seq(
      repeat($.decorator), optional($._visibility),
      'class', field('name', $.identifier), optional($.type_params),
      '{', repeat(choice($._member, ',')), '}',
    ),

    _member: $ => choice($.field_decl, $.function_decl, $.prop_decl),

    field_decl: $ => seq(
      optional($._visibility), optional(choice('let', 'var')),
      field('name', $.identifier), ':', $._type,
    ),

    prop_decl: $ => seq(
      optional($._visibility), 'prop', field('name', $.identifier),
      ':', $._type, optional($._block),
    ),

    enum_decl: $ => seq(
      repeat($.decorator), optional($._visibility),
      'enum', field('name', $.identifier), optional($.type_params),
      '{', repeat(choice($.variant, $.line_comment)), '}',
    ),

    variant: $ => seq(
      field('name', $.identifier),
      optional(','),
      optional(choice(
        seq('(', optional(commaSep1($._type)), optional(','), ')'),
        seq('{', repeat(choice($.field_decl, ',')), '}'),
      )),
    ),

    trait_decl: $ => seq(
      repeat($.decorator),
      'trait', field('name', $.identifier), optional($.type_params),
      optional(seq(':', $.bound)),
      '{', repeat(choice($._member, ',')), '}',
    ),

    impl_decl: $ => seq(
      'impl', optional($.type_params),
      $.path, optional($.type_args), 'for', $._type,
      '{', repeat(choice($.function_decl, $.prop_decl)), '}',
    ),

    function_decl: $ => seq(
      repeat($.decorator), optional($._visibility), optional('comptime'),
      optional(seq('extern', $.string)),
      'fn', field('name', $.identifier), optional($.type_params),
      '(', optional(commaSep1($.param)), optional(','), ')',
      optional(seq('->', $._type)),
      optional($._block),
    ),

    param: $ => choice(
      seq('&', 'self'),
      seq('var', 'self'),
      'self',
      '...',
      seq(optional('var'), field('name', $.identifier), ':', $._type),
    ),

    const_decl: $ => seq('const', field('name', $._name), ':', $._type, '=', $._expr),
    static_decl: $ => seq('static', 'let', field('name', $._name), ':', $._type, '=', $._expr),

    test_decl: $ => seq('test', $.string, $._block),

    // ── GUI 视图/样式(tests/gui 语料:CTML 内嵌于 .ct)──
    view_decl: $ => seq(
      optional($._visibility), 'view', field('name', $.identifier),
      optional(seq('(', /[^{}()]*/, ')')),
      // 语料实证:ev 事件子句(view Card (...) ev fold() {})
      optional(seq('ev', $.identifier, '(', /[^()]*/, ')')),
      '{', repeat(choice($.tag, $.tag_close, $.interpolation, $.line_comment, $.markup_text)), '}',
    ),

    tag: $ => seq(
      '<', field('name', $.tag_name),
      repeat(choice($.tag_attr, $.string, $.interpolation, $.line_comment)),
      optional('/'), '>',
    ),

    tag_close: $ => seq('</', $.tag_name, optional('/'), '>'),

    tag_name: _ => /[A-Za-z][A-Za-z0-9_-]*/,

    tag_attr: $ => seq(
      /[a-zA-Z_:-][A-Za-z0-9_:-]*/,
      optional(seq('=', optional(choice($.string, $.interpolation)))),
    ),

    markup_text: _ => token(prec(-1, /[^<>{}]+/)),

    style_decl: $ => seq(
      optional($._visibility), 'style', field('name', /[a-z][a-z0-9_-]*/),
      optional(seq('extends', /[a-z][a-z0-9_-]*/)),
      '{', repeat(choice($.line_comment, $.style_prop, $.string, $.number, $.style_const, $.style_word)), '}',
    ),

    // 无重叠形态:属性名带冒号成单 token(direction:),与值词(column)词法互斥
    style_prop: _ => token(/[a-z][a-z0-9-]*:/),
    style_word: _ => token(/[a-z][a-z0-9-]*/),
    style_const: _ => token(/[A-Z_][A-Z0-9_]*/),

    // ── 属性(§8.3)──
    decorator: $ => choice(
      seq('#[', /[A-Za-z_][A-Za-z0-9_.]*/,
        // 实参单 token 容错(kv/表达式竞争在 GLR 下不稳,实参内细粒度着色让位)
        // 允许一层嵌套括号:repr(c, aligned(8))
        optional(seq('(', /([^()\n]*\([^()\n]*\)[^()\n]*|[^()\n]*)/, ')')),
      ']'),
      seq('@derive', '(', commaSep1($.path), ')'),
    ),

    _visibility: $ => choice('pub', seq('pub', '(', 'pkg', ')')),  // §2.3

    // ── 泛型与约束 ──
    type_params: $ => seq('[', commaSep1(choice($.type_param, seq('comptime', $.type_param))), ']'),
    type_param: $ => seq($._name, optional(seq(':', $.bound))),
    bound: $ => seq($.path, repeat(seq('+', $.path))),
    path: $ => prec(1, seq($._name, repeat(seq('.', $._name)))),
    _name: $ => choice($.identifier, $.identifier),

    // ── 语句(§1.7 Stmt;换行定界按空白处理,见文件头说明)──
    _block: $ => seq('{', repeat(choice($._stmt, $._expr)), '}'),

    _stmt: $ => choice(
      $.let_stmt,
      $.var_stmt,
      $.return_stmt,
      $.for_stmt,
      $.while_stmt,
      $.assign_stmt,
    ),

    // AssignStmt = PostfixExpr AssignOp Expr(§1.7)
    assign_stmt: $ => seq(
      choice($.identifier, $.member_expression, $.subscript_expression),
      field('operator', choice('=', '+=', '-=', '*=', '/=', '%=')),
      $._expr,
    ),

    let_stmt: $ => seq('let', $._pattern, optional(seq(':', $._type)), '=', $._expr),
    var_stmt: $ => seq('var', $._pattern, optional(seq(':', $._type)), '=', $._expr),
    return_stmt: $ => seq('return', optional($._expr)),
    for_stmt: $ => seq('for', $._pattern, 'in', $._expr, $._block),
    while_stmt: $ => seq('while', $._expr, $._block),

    // ── 模式 ──
    _pattern: $ => choice(
      $.number, $.string, $.boolean, $.identifier, $.wildcard,
      $.tuple_pattern, $.aggregate_pattern, $.or_pattern,
    ),

    // 语料实证:"go" | "ctron" / A | B(§1.7 Pattern 之外的真实超集)
    or_pattern: $ => prec.left(1, seq($._pattern, '|', $._pattern)),

    wildcard: $ => '_',

    tuple_pattern: $ => prec(1, seq('(', optional(commaSep1($._pattern)), optional(','), ')')),

    aggregate_pattern: $ => seq(
      $.path,
      optional(choice(
        seq('(', optional(commaSep1($._pattern)), optional(','), ')'),
        seq('{', repeat(choice($.field_pattern, ',')), '}'),
      )),
    ),

    field_pattern: $ => seq(field('name', $.identifier), optional(seq(':', $._pattern))),

    // ── 表达式(§1.7 优先级阶梯;? 为后缀)──
    _expr: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $.try_expression,
      $.call_expression,
      $.subscript_expression,
      $.member_expression,
      $._primary,
    ),

    binary_expression: $ => {
      const levels = [
        [['||'], 1],
        [['or'], 2],
        [['&&'], 3],
        [['==', '!=', '<', '>', '<=', '>='], 4],
        [['..', '..='], 5],
        [['+', '-', '+%', '-%'], 6],
        [['*', '/', '%', '**'], 7],
      ];
      return choice(
        ...levels.flatMap(([operators, precedence]) =>
          operators.map((operator) =>
            prec.left(precedence, seq($._expr, field('operator', operator), $._expr)))),
      );
    },

    unary_expression: $ => prec(8, seq(choice('-', '!'), $._expr)),
    try_expression: $ => prec(9, seq($._expr, '?')),
    // 被调限定为可调用形态,防止上一臂的字面量与下一臂 `(p, q)` 粘成调用
    call_expression: $ => prec(10, seq(
      field('function', choice(
        $.identifier,
        $.identifier,
        $.member_expression,
        $.subscript_expression,
        $.call_expression,
        $.parenthesized_expression,
      )),
      $.argument_list,
    )),
    argument_list: $ => seq('(', optional(commaSep1(choice($._expr, $.labeled_arg))), optional(','), ')'),
    // 语料实证:TodoApp(m: make()) —— 具名实参(构造调用位)
    labeled_arg: $ => prec(1, seq($.identifier, ':', $._expr)),
    // [ 后缀统一为 subscript:索引与泛型实参(Simd[F32, 4],§1.8)真歧义,
    // 高亮无需区分,单规则持有 [ 消灭冲突族
    subscript_expression: $ => prec(10, seq(
      $._expr, '[', repeat(choice($._expr, $._type, $.number, ',')), ']',
    )),
    member_expression: $ => prec(11, seq($._expr, '.', field('member', choice($.identifier, $.number)))),

    _primary: $ => choice(
      $.identifier, $.identifier, $.number, $.string, $.boolean, $.self, $.void,
      $.parenthesized_expression, $.array_literal, $.struct_literal,
      $.if_expression, $.match_expression, $.closure, $.scope_expression,
      $.own_expression, $._block,
    ),

    parenthesized_expression: $ => seq('(', optional(commaSep1($._expr)), optional(','), ')'),
    array_literal: $ => seq('[', optional(commaSep1($._expr)), optional(','), ']'),

    struct_literal: $ => seq(
      $.path, optional($.type_args),
      '{', repeat(choice($.field_init, ',')), '}',
    ),

    field_init: $ => seq(field('name', $.identifier), optional(seq(':', $._expr))),

    if_expression: $ => seq(
      'if', $._expr, $._block,
      optional(seq('else', choice($.if_expression, $._block))),
    ),

    match_expression: $ => prec.right(1, seq('match', $._expr, '{', repeat($.match_arm), '}')),
    // 守卫为语料实证形态(x if x < 0 => …),spec §1.7 的 MatchArm 之外的真实超集
    match_arm: $ => seq(
      $._pattern,
      optional(seq('if', $._expr)),
      '=>',
      $._expr,
      optional(','),
    ),

    closure: $ => choice(
      seq('||', optional(seq('->', $._type)), $._expr),
      seq(
        '|',
        optional(commaSep1($.closure_param)),
        '|',
        optional(seq('->', $._type)),
        $._expr,
      ),
    ),

    closure_param: $ => seq(optional('var'), $.identifier, optional(seq(':', $._type))),

    scope_expression: $ => seq(
      'scope', '{',
      // §1.7 ClosureBlock:"{" "|" IDENT "|" { Stmt|Expr } [Expr] "}"
      optional(seq('|', optional(commaSep1($.closure_param)), '|')),
      repeat(choice($._stmt, $._expr)),
      '}',
    ),
    own_expression: $ => seq('own', '(', $.identifier, ')', $._block),

    // ── 类型(§1.7 Type;[...] 三态全接受)──
    _type: $ => choice(
      $.reference_type,
      $.slice_type,
      $.array_type,
      $.optional_type,
      $.tuple_type,
      $.function_type,
      $.path_type,
      'self',
    ),

    reference_type: $ => prec.left(12, seq('&', $._type)),
    slice_type: $ => prec(13, seq($._type, '[', ']')),
    array_type: $ => prec(13, seq($._type, '[', $._expr, ']')),
    optional_type: $ => prec(14, seq($._type, '?')),
    tuple_type: $ => seq('(', optional(commaSep1($._type)), optional(','), ')'),
    function_type: $ => prec.left(12, seq(
      'fn', '(', optional(commaSep1($._type)), ')', optional(seq('->', $._type)),
    )),
    path_type: $ => prec(15, seq($.path, optional($.type_args))),

    type_args: $ => seq('[', commaSep1(choice($._type, $.number)), ']'),

    // ── 词法(§1.3/§1.4)──
    // 单 token 收大小写:命名约定是 lint(§1.3)非语法,PascalCase 函数名(如 Net_probe)必须可解析
    identifier: $ => /_?[A-Za-z_][A-Za-z0-9_]*/,

    line_comment: _ => token(prec(1, seq('//', /[^\n]*/))),

    number: _ => token(choice(FLOAT, INT)),

    boolean: _ => choice('true', 'false'),
    void: _ => 'void',
    self: _ => 'self',

    string: $ => seq(
      '"',
      repeat(choice($.escape_sequence, $.interpolation, $.string_content)),
      '"',
    ),

    escape_sequence: _ => token(/\\[ntr\\"0{]|\\u\{[0-9a-fA-F_]+\}/),
    string_content: _ => token(prec(2, /[^"\\{]+/)),

    interpolation: $ => seq('{', optional(choice($.interp_expr, $.interp_text)), '}'),

    // 插值子集(vscode 同款):标识符/字段链/索引/调用/简单二元 —— 全为正向 token,
    // 任一失败即整体回落 interp_text,避免"半匹配 + ERROR 节点"
    interp_expr: $ => prec(1, seq(
      $._interp_atom,
      repeat(choice(
        seq('.', choice($.identifier, $.number)),
        seq('[', $.interp_expr, ']'),
        seq('(', optional(commaSep1(choice($._interp_value, $.interp_expr))), ')'),
        seq(choice('?', '+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=', '&&', '||'), $._interp_value),
      )),
    )),

    _interp_atom: $ => choice($.identifier, $.number, $.string, $.boolean),
    _interp_value: $ => choice($._interp_atom, seq('-', $.number)),

    interp_text: _ => token(prec(-1, /[^{}"\\]+/)),
  },
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(',', rule)));
}

function commaSep(rule) {
  return optional(commaSep1(rule));
}
