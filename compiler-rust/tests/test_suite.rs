//! R-P2c 集成验收:`ctron test` —— 发现/过滤/panic 标记/JSON/pkg 目录/退出码。
//! lib 级走 ctron::testing::test_report;CLI 级走 CARGO_BIN_EXE_ctron。

use ctron::sem::Profile;
use ctron::testing::{test_report, TestStatus};

const SIMPLE: &str = r#"
test "alpha" {
    assert_eq(1 + 1, 2)
}

test "beta" {
    assert_eq(2 * 2, 4)
}
"#;

// ---------- lib 级:发现 ----------

#[test]
fn test_blocks_are_discovered_and_pass() {
    let report = test_report(SIMPLE, Profile::Full, None);
    assert!(report.diags.is_empty(), "不应有解析诊断: {:?}", report.diags);
    assert_eq!(report.results.len(), 2);
    assert_eq!(report.results[0].name, "alpha");
    assert_eq!(report.results[0].status, TestStatus::Pass);
    assert_eq!(report.results[1].name, "beta");
    assert_eq!(report.results[1].status, TestStatus::Pass);
}

#[test]
fn zero_arg_fn_test_is_discovered_but_ordinary_fn_is_not() {
    let src = r#"
fn test_helper() -> I32 {
    return 42
}

fn helper() -> I32 {
    return 1
}

fn test_with_args(x: I32) -> I32 {
    return x
}

test "block case" {
    assert_eq(true, true)
}
"#;
    let report = test_report(src, Profile::Full, None);
    assert!(report.diags.is_empty(), "不应有解析诊断: {:?}", report.diags);
    let names: Vec<&str> = report.results.iter().map(|r| r.name.as_str()).collect();
    assert_eq!(names, vec!["block case", "test_helper"], "零参 fn test_* 入发现,普通 fn 与带参 fn 不入");
}

#[test]
fn filter_keeps_substring_matches_only() {
    let report = test_report(SIMPLE, Profile::Full, Some("alp"));
    assert_eq!(report.results.len(), 1);
    assert_eq!(report.results[0].name, "alpha");
}

// ---------- lib 级:失败与 panic 标记 ----------

#[test]
fn failing_assert_reports_fail_with_message() {
    let src = r#"
test "boom" {
    assert_eq(1, 2)
}
"#;
    let report = test_report(src, Profile::Full, None);
    assert_eq!(report.results.len(), 1);
    assert_eq!(report.results[0].status, TestStatus::Fail);
    assert!(report.results[0].message.is_some(), "失败必须带消息");
}

#[test]
fn panic_marker_turns_matching_panic_into_panic_ok() {
    let src = "//@ panic: overflow\n\ntest \"explodes\" {\n    let m: U8 = 255\n    let _ = m + 1u8\n}\n";
    let report = test_report(src, Profile::Full, None);
    assert_eq!(report.results.len(), 1);
    assert_eq!(report.results[0].status, TestStatus::PanicOk);
}

#[test]
fn without_marker_a_panic_is_a_failure() {
    let src = "test \"explodes\" {\n    let m: U8 = 255\n    let _ = m + 1u8\n}\n";
    let report = test_report(src, Profile::Full, None);
    assert_eq!(report.results[0].status, TestStatus::Fail);
}

// ---------- CLI 级 ----------

fn run_cli(args: &[&str]) -> (String, String, Option<i32>) {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_ctron"))
        .args(args)
        .output()
        .expect("ctron 二进制可执行");
    (
        String::from_utf8_lossy(&out.stdout).into_owned(),
        String::from_utf8_lossy(&out.stderr).into_owned(),
        out.status.code(),
    )
}

fn write_tempfile(tag: &str, content: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir();
    let path = dir.join(format!("ctron_test_suite_{}_{}.ct", tag, std::process::id()));
    std::fs::write(&path, content).unwrap();
    path
}

#[test]
fn cli_all_pass_exit_zero_with_summary() {
    let path = write_tempfile("pass", SIMPLE);
    let (stdout, _, code) = run_cli(&["test", path.to_str().unwrap()]);
    assert_eq!(code, Some(0));
    assert!(stdout.contains("ok   alpha"), "实际输出: {stdout}");
    assert!(stdout.contains("ok   beta"));
    assert!(stdout.contains("2 passed"), "应有汇总行: {stdout}");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_failure_exit_one_and_fail_line() {
    let src = "test \"good\" {\n    assert_eq(1, 1)\n}\n\ntest \"bad\" {\n    assert_eq(1, 2)\n}\n";
    let path = write_tempfile("fail", src);
    let (stdout, stderr, code) = run_cli(&["test", path.to_str().unwrap()]);
    assert_eq!(code, Some(1));
    assert!(stdout.contains("ok   good"));
    assert!(stdout.contains("FAIL bad"));
    assert!(stderr.contains("1 个测试失败"), "stderr 汇总: {stderr}");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_filter_narrows_selection() {
    let path = write_tempfile("filter", SIMPLE);
    let (stdout, _, code) = run_cli(&["test", path.to_str().unwrap(), "--filter", "beta"]);
    assert_eq!(code, Some(0));
    assert!(stdout.contains("ok   beta"));
    assert!(!stdout.contains("alpha"), "--filter 后不得跑未匹配项: {stdout}");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_json_format_has_contract_fields() {
    let path = write_tempfile("json", SIMPLE);
    let (stdout, _, code) = run_cli(&["test", path.to_str().unwrap(), "--format=json"]);
    assert_eq!(code, Some(0));
    for needle in ["\"total\": 2", "\"passed\": 2", "\"failed\": 0", "\"name\": \"alpha\"", "\"status\": \"ok\""] {
        assert!(stdout.contains(needle), "JSON 缺 {needle}: {stdout}");
    }
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_deterministic_flag_is_accepted() {
    let path = write_tempfile("det", SIMPLE);
    let (_, _, code) = run_cli(&["test", path.to_str().unwrap(), "--deterministic"]);
    assert_eq!(code, Some(0), "--deterministic 为接受占位,不得报用法错");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_pkg_dir_runs_sorted_sources() {
    let base = std::env::temp_dir().join(format!("ctron_test_pkg_{}", std::process::id()));
    let src_dir = base.join("src");
    std::fs::create_dir_all(&src_dir).unwrap();
    std::fs::write(base.join("Ctron.ctcl"), "pkg {\n    manifest_version = 1\n    name = \"mylib\"\n}\n").unwrap();
    std::fs::write(src_dir.join("a_math.ct"), "test \"pkg math\" {\n    assert_eq(1, 1)\n}\n").unwrap();
    std::fs::write(src_dir.join("b_str.ct"), "test \"pkg str\" {\n    assert_eq(\"a\", \"a\")\n}\n").unwrap();
    let (stdout, _, code) = run_cli(&["test", base.to_str().unwrap()]);
    assert_eq!(code, Some(0), "pkg 目录测试应全过: {stdout}");
    assert!(stdout.contains("ok   pkg math"));
    assert!(stdout.contains("ok   pkg str"));
    assert!(stdout.contains("2 passed"));
    let _ = std::fs::remove_dir_all(&base);
}

#[test]
fn cli_parse_error_exit_one_with_diagnostic() {
    let path = write_tempfile("neg", "test \"broken\" {\n    let = 3\n}\n");
    let (stdout, _, code) = run_cli(&["test", path.to_str().unwrap()]);
    assert_eq!(code, Some(1), "解析诊断必须非零退出: {stdout}");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_usage_error_exit_two() {
    let (_, stderr, code) = run_cli(&["test"]);
    assert_eq!(code, Some(2));
    assert!(stderr.contains("usage: ctron test"));
}
