//! P1-B 集成验收:tests/ 全部 .ct 文件过解析器。
//! 预期:仅 01c_parse.neg.ct 报 E1001、06_static_var.neg.ct 报 E3030,其余零诊断。

use std::path::{Path, PathBuf};

fn walk_ct(dir: &Path, out: &mut Vec<PathBuf>) {
    // 只认 tests/ 顶层单文件:泳道子目录(net/http/doc_fix/artifact_demo/…)
    // 各有专属 runner 且系多文件包结构,单文件解析必 E2020/E1001 误报
    // (对齐 meta_check 泳道 skip 口径)
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if !p.is_dir() && p.extension().is_some_and(|x| x == "ct") {
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
        // roadmap/ 锚点语料的解析状态由 roadmap_suite 按锚定状态驱动(测试先行)
        if let Ok(rel) = f.strip_prefix(&root) {
            if rel.starts_with("roadmap/") { continue; }
        }
        let src = std::fs::read_to_string(f).unwrap();
        let (_, diags) = ctron::parse_src(&src);
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let parent = f.parent().and_then(|p| p.file_name()).map(|p| p.to_string_lossy().to_string()).unwrap_or_default();
        let key = format!("{}/{}", parent, name);
        let cs = codes(&diags);
        let neg_want: Option<&[&'static str]> = if name == "01c_parse.neg.ct" {
            Some(&["E1001"])
        } else if name == "01i_semicolon.neg.ct" {
            Some(&["E1001"])
        } else if name == "01j_impl_for.neg.ct" {
            Some(&["E1001"])
        } else if name == "06_static_var.neg.ct" {
            Some(&["E3030"])
        } else {
            None
        };
        // neg 文件:必须包含期望码;其余:必须零诊断
        let ok = match neg_want {
            Some(want) => want.iter().all(|w| cs.contains(w)),
            None => diags.is_empty(),
        };
        if !ok {
            let detail = diags.iter().map(|d| format!("  {}:{}:{} {}: {}", f.display(), d.span.line, d.span.col, d.code, d.message)).collect::<Vec<_>>().join("\n");
            failures.push(format!("{} — 诊断 {} 条:\n{}", key, diags.len(), if detail.is_empty() { "  (无)".into() } else { detail }));
        }
    }
    assert!(failures.is_empty(), "解析不符合预期的文件({}):\n{}", failures.len(), failures.join("\n---\n"));
}
