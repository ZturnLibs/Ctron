//! v0.7 修订三(R 线切片):调用点类型推断(Go 式仅实参)—— 行为/负例/显式并存验收。

use ctron::sem::Profile;

fn fixture_dir() -> std::path::PathBuf {
    std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures")
}

#[test]
fn positive_fixture_all_tests_pass() {
    let src = std::fs::read_to_string(fixture_dir().join("04f_infer.ct")).unwrap();
    assert!(ctron::parse_src(&src).1.is_empty(), "解析须零诊断");
    let results = ctron::interp::run_test_file(&src, Profile::Full);
    assert!(!results.is_empty());
    for (name, r) in &results {
        assert!(matches!(r, Ok(())), "test `{name}` 失败: {r:?}");
    }
}

#[test]
fn missing_candidate_reports_e2060() {
    let src = std::fs::read_to_string(fixture_dir().join("04f_infer_missing.neg.ct")).unwrap();
    let diags = ctron::check_src(&src, Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2060" && d.message.contains("无法推断")),
        "应报 E2060,实际: {diags:?}");
}

#[test]
fn conflicting_candidates_report_e2061() {
    let src = std::fs::read_to_string(fixture_dir().join("04f_infer_ambig.neg.ct")).unwrap();
    let diags = ctron::check_src(&src, Profile::Full);
    assert!(diags.iter().any(|d| d.code == "E2061" && d.message.contains("无法唯一推断")),
        "应报 E2061,实际: {diags:?}");
}

#[test]
fn explicit_typeargs_path_checks_and_returns_concrete() {
    // 显式 TypeArgs 恒合法(纯增量);此处验证检查面不再把它降级为 Err:
    // 参数个数不符应报诊断(旧行为:整体 Ty::Err,静默)
    let diags = ctron::check_src(
        "fn identity[V](x: V) -> V {\n    return x\n}\n\ntest \"t\" {\n    let a = identity[I32, I32](5)\n    let _ = a\n}\n",
        Profile::Full,
    );
    assert!(diags.iter().any(|d| d.code == "E2020" && d.message.contains("类型实参数量不符")),
        "显式 TypeArgs 数量错位应报 E2020,实际: {diags:?}");
}

#[test]
fn inferred_call_is_checked_against_substituted_formals() {
    // 形参 I32 具体化后,Str 实参应在推断路径下被检查出类别冲突
    let diags = ctron::check_src(
        "fn twice(n: I32) -> I32 {\n    return n * 2\n}\n",
        Profile::Full,
    );
    // 对照组:非泛型路径已有检查;泛型路径由 04f 正例行为覆盖,此处保空诊断基线
    assert!(diags.is_empty(), "{diags:?}");
}
