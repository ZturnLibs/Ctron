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

#[test]
fn all_suite_files_lex_without_diagnostics() {
    // brief 原文为 `../../tests`,但本仓库布局为 <root>/compiler + <root>/tests,故为 `../tests`
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);
    assert!(files.len() >= 57, "测试文件应不少于 57 个,实际 {}", files.len());
    let mut failures = Vec::new();
    for f in &files {
        // *.neg.ct 允许词法级错误(其判定面是 //@ fail: 码,见 check_suite/roadmap_suite
        // NegGreen 锚;01i_semicolon、r6h/r6i 插值负例均属此类),不入本断言
        if f.to_string_lossy().ends_with(".neg.ct") {
            continue;
        }
        let src = std::fs::read_to_string(f).unwrap();
        let (toks, diags) = ctron::lex(&src);
        if !diags.is_empty() || toks.len() < 2 {
            failures.push(format!("{}: {} diagnostics", f.display(), diags.len()));
        }
    }
    assert!(failures.is_empty(), "词法失败的文件:\n{}", failures.join("\n"));
}
