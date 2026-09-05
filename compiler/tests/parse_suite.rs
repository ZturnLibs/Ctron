//! P1-B 集成验收:tests/ 全部 .ct 文件过解析器。
//! 预期:仅 01c_parse.neg.ct 报 E1001、06_static_var.neg.ct 报 E3030,其余零诊断。

use std::path::{Path, PathBuf};

fn walk_ct(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if p.is_dir() {
            walk_ct(&p, out);
        } else if p.extension().is_some_and(|x| x == "ct") {
            out.push(p);
        }
    }
}

fn codes(diags: &[ctron::token::Diagnostic]) -> Vec<&'static str> {
    diags.iter().map(|d| d.code).collect()
}

#[test]
fn all_suite_files_parse_per_expectation() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);
    assert!(files.len() >= 61, "测试文件应不少于 61 个,实际 {}", files.len());

    let mut failures = Vec::new();
    for f in &files {
        let src = std::fs::read_to_string(f).unwrap();
        let (_, diags) = ctron::parse_src(&src);
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let parent = f.parent().and_then(|p| p.file_name()).map(|p| p.to_string_lossy().to_string()).unwrap_or_default();
        let key = format!("{}/{}", parent, name);
        let cs = codes(&diags);
        let expected: Option<&[&'static str]> = if name == "01c_parse.neg.ct" {
            Some(&["E1001"])
        } else if name == "06_static_var.neg.ct" {
            Some(&["E3030"])
        } else {
            Some(&[])
        };
        let ok = match expected {
            Some(want) => want.iter().all(|w| cs.contains(w)) && (want.is_empty() || cs.len() == want.len() || want.iter().all(|w| cs.contains(w))),
            None => cs.is_empty(),
        };
        // 对两个 neg 文件:必须包含期望码;对其余:必须零诊断
        let ok = if name == "01c_parse.neg.ct" || name == "06_static_var.neg.ct" {
            expected.unwrap().iter().all(|w| cs.contains(w))
        } else {
            diags.is_empty()
        };
        if !ok {
            let detail = diags.iter().map(|d| format!("  {}:{}:{} {}: {}", f.display(), d.span.line, d.span.col, d.code, d.message)).collect::<Vec<_>>().join("\n");
            failures.push(format!("{} — 诊断 {} 条:\n{}", key, diags.len(), if detail.is_empty() { "  (无)".into() } else { detail }));
        }
    }
    assert!(failures.is_empty(), "解析不符合预期的文件({}):\n{}", failures.len(), failures.join("\n---\n"));
}
