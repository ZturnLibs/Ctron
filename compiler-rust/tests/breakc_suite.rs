//! v0.7 修订二(R 线切片):break/continue —— 行为/负例/fmt/C 发射验收。
//! 语料置于 compiler-rust/tests/fixtures/(R 线本地,移植自举线时提升共享)。

use ctron::sem::Profile;

fn fixture_dir() -> std::path::PathBuf {
    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures")
}

fn run_fixture(name: &str) -> Vec<(String, Result<(), String>)> {
    let src = std::fs::read_to_string(fixture_dir().join(name)).unwrap();
    ctron::interp::run_test_file(&src, Profile::Full)
}

#[test]
fn positive_fixture_all_tests_pass() {
    let results = run_fixture("04e_break_continue.ct");
    assert!(!results.is_empty());
    for (name, r) in &results {
        assert!(matches!(r, Ok(())), "test `{name}` 失败: {r:?}");
    }
}

#[test]
fn break_outside_loop_reports_e2070() {
    let src = std::fs::read_to_string(fixture_dir().join("04e_break_outside.neg.ct")).unwrap();
    let diags = ctron::check_src(&src, Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2070" && d.message.contains("break")),
        "应报 E2070,实际: {diags:?}");
}

#[test]
fn continue_outside_loop_reports_e2070() {
    let diags = ctron::check_src("test \"c\" {\n    continue\n}\n", Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2070" && d.message.contains("continue")),
        "应报 E2070,实际: {diags:?}");
}

#[test]
fn break_through_closure_reports_e2072() {
    let src = std::fs::read_to_string(fixture_dir().join("04e_break_closure.neg.ct")).unwrap();
    let diags = ctron::check_src(&src, Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2072" && d.message.contains("闭包")),
        "应报 E2072,实际: {diags:?}");
}

#[test]
fn fmt_keeps_break_continue_layout() {
    let src = "test \"t\" {\n    var i: I32 = 0\n    while i < 5 {\n        i += 1\n        if i == 3 { break }\n        if i == 1 { continue }\n    }\n}\n";
    assert_eq!(ctron::fmt::fmt_src(src).unwrap(), src);
}

#[test]
fn trans_emits_c_break_and_continue() {
    let src = "fn f() {\n    var i: I32 = 0\n    while i < 5 {\n        i += 1\n        if i == 3 { break }\n        if i == 1 { continue }\n    }\n}\n";
    let (file, diags) = ctron::parse_src(src);
    assert!(diags.is_empty(), "{diags:?}");
    let code = ctron::trans::Trans::new().trans_files(std::slice::from_ref(&file)).unwrap();
    assert!(code.contains("break;"), "应有 C break: {}", &code[code.len() - 800..]);
    assert!(code.contains("continue;"), "应有 C continue");
}
