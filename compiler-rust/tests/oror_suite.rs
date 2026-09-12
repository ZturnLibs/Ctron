//! v0.7 修订一(R 线切片):`||` 逻辑或 —— 行为/负例/fmt 位置消歧验收。
//! 语料置于 compiler-rust/tests/fixtures/(R 线本地):
//! 共享 tests/ 由自举线 meta_check 以种子编译器扫描,`||` 移植自举线前不得入内。

use ctron::sem::Profile;

fn fixture_dir() -> std::path::PathBuf {
    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures")
}

#[test]
fn positive_fixture_all_tests_pass() {
    let path = fixture_dir().join("04d_bool_or.ct");
    let src = std::fs::read_to_string(&path).unwrap();
    assert!(ctron::parse_src(&src).1.is_empty(), "解析须零诊断");
    let results = ctron::interp::run_test_file(&src, Profile::Full);
    assert!(!results.is_empty());
    for (name, r) in &results {
        assert!(matches!(r, Ok(())), "test `{name}` 失败: {r:?}");
    }
}

#[test]
fn neg_fixture_reports_e2010_with_bool_message() {
    let path = fixture_dir().join("04d_bool_or_type.neg.ct");
    let src = std::fs::read_to_string(&path).unwrap();
    let diags = ctron::check_src(&src, Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2010" && d.message.contains("需要 Bool")),
        "应报 E2010 `||` 需要 Bool,实际: {diags:?}");
    // 取默认提示(宪法:Option/Result 语义走 or)
    assert!(diags.iter().any(|d| d.message.contains("取默认请用")),
        "Option 操作数应提示用 or 取默认,实际: {diags:?}");
}

#[test]
fn lexer_oror_and_newline_continuation() {
    // 中缀:独立 token
    let (t, d) = ctron::lex("a || b");
    assert!(d.is_empty());
    let kinds: Vec<String> = t.iter().map(|x| format!("{:?}", x.tok)).collect();
    assert!(kinds.iter().any(|k| k == "OrOr"), "应有 OrOr: {kinds:?}");
    // 行尾 || 为延续:换行被 §1.6 滤除,表达式连续
    let (t2, d2) = ctron::lex("let x = a ||\n    b\n");
    assert!(d2.is_empty());
    assert_eq!(t2.iter().filter(|x| x.tok == ctron::token::Tok::Newline).count(), 1,
        "|| 后换行应被滤除,仅剩语句尾换行: {t2:?}");
    // 零参闭包:起始位 ||
    let (t3, d3) = ctron::lex("pick(|| 7)");
    assert!(d3.is_empty());
    let kinds3: Vec<String> = t3.iter().map(|x| format!("{:?}", x.tok)).collect();
    assert!(kinds3.contains(&"OrOr".to_string()), "起始位 || 应为 OrOr: {kinds3:?}");
}

#[test]
fn fmt_keeps_or_spacing_and_closure_disambiguation() {
    let src = "fn f(a: Bool, b: Bool) -> Bool {\n    return a || b\n}\n";
    assert_eq!(ctron::fmt::fmt_src(src).unwrap(), src);
    let src2 = "fn g() -> I32 {\n    return pick(|| 7)\n}\n";
    assert_eq!(ctron::fmt::fmt_src(src2).unwrap(), src2);
    // 跨行延续:行尾 || 后接续行,fmt 单行化后重词法化等价
    let src3 = "fn h(a: Bool,\n        b: Bool) -> Bool {\n    return a ||\n        b\n}\n";
    let out3 = ctron::fmt::fmt_src(src3).unwrap();
    assert!(out3.contains("a || b") || out3.contains("a ||\n"), "合法续行排版: {out3}");
}
