//! 测试报告装配(R-P2c):发现 + 运行 + panic 标记 + 过滤。
//! CLI(`ctron test`)只做渲染;`run` 子命令保持原样(campaign.py 兼容)。

use crate::sem::Profile;
use crate::token::Diagnostic;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TestStatus { Pass, Fail, PanicOk }

impl TestStatus {
    pub fn as_str(&self) -> &'static str {
        match self { TestStatus::Pass => "ok", TestStatus::Fail => "fail", TestStatus::PanicOk => "panic-ok" }
    }
}

#[derive(Debug, Clone)]
pub struct TestResult {
    pub name: String,
    pub status: TestStatus,
    pub message: Option<String>,
}

#[derive(Debug)]
pub struct TestReport {
    pub results: Vec<TestResult>,
    pub diags: Vec<Diagnostic>,
}

impl TestReport {
    pub fn passed(&self) -> usize {
        self.results.iter().filter(|r| matches!(r.status, TestStatus::Pass | TestStatus::PanicOk)).count()
    }
    pub fn failed(&self) -> usize {
        self.results.iter().filter(|r| r.status == TestStatus::Fail).count()
    }
}

/// 文件级 `//@ panic: 标记`:结果 panic 且消息含标记 → PanicOk;
/// 声明了标记却无任何匹配 panic → 追加一条 Fail(对齐 run 子命令口径)。
pub fn test_report(src: &str, profile: Profile, filter: Option<&str>) -> TestReport {
    let panic_marker = src.lines()
        .find(|l| l.trim_start().starts_with("//@ panic:"))
        .map(|l| l.trim_start_matches("//@ panic:").trim().to_string());
    let (_, diags) = crate::parse_src(src);
    if !diags.is_empty() {
        return TestReport { results: Vec::new(), diags };
    }
    let raw = crate::interp::run_test_file(src, profile);
    let mut results = Vec::new();
    let mut any_panic_ok = false;
    for (name, r) in raw {
        if let Some(f) = filter {
            if !name.contains(f) { continue; }
        }
        let (status, message) = match r {
            Ok(()) => (TestStatus::Pass, None),
            Err(m) => {
                let expected = panic_marker.as_ref().is_some_and(|mk| m.contains(mk.as_str()));
                if expected { any_panic_ok = true; (TestStatus::PanicOk, Some(m)) }
                else { (TestStatus::Fail, Some(m)) }
            }
        };
        results.push(TestResult { name, status, message });
    }
    if let Some(marker) = &panic_marker {
        if !any_panic_ok && results.iter().all(|r| r.status != TestStatus::Fail) {
            results.push(TestResult {
                name: format!("预期 panic(含 \"{marker}\")"),
                status: TestStatus::Fail,
                message: Some("未发生匹配的 panic".into()),
            });
        }
    }
    TestReport { results, diags: Vec::new() }
}

/// JSON 字符串转义(无 serde 依赖,覆盖报告面出现的字符域)
pub fn json_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}
