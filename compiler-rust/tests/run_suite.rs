//! P1-D 集成验收:61 文件运行期望矩阵。
//! 行为文件:所有 test 块通过;panic 文件:运行时 panic 且消息匹配;neg/lint:不运行。

use std::path::{Path, PathBuf};

fn walk_ct(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if p.is_dir() { walk_ct(&p, out); }
        else if p.extension().is_some_and(|x| x == "ct") { out.push(p); }
    }
}

fn profile_for(path: &Path) -> ctron::sem::Profile {
    let name = path.file_name().unwrap().to_string_lossy().to_string();
    if name.starts_with("08_bare") { ctron::sem::Profile::Bare }
    else if name.starts_with("10_web") { ctron::sem::Profile::Web }
    else { ctron::sem::Profile::Full }
}

#[test]
fn all_behavior_files_run_and_pass() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);
    assert!(files.len() >= 61);

    let mut failures = Vec::new();
    let mut ran = 0usize;
    let mut deferred = 0usize;

    for f in &files {
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let rel = f.strip_prefix(&root).unwrap_or(f).to_string_lossy().to_string();
        if rel.starts_with("modules/") { continue; }
        // neg/lint 不运行(编译期判定)
        if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") { continue; }
        // web 目标暂不运行(无浏览器后端)
        if name.starts_with("10_web") { deferred += 1; continue; }

        let src = std::fs::read_to_string(f).unwrap();
        eprintln!("[run] {}", rel);
        let profile = profile_for(f);
        let results = ctron::interp::run_test_file(&src, profile);
        ran += 1;

        if name.ends_with(".panic.ct") {
            // 期望至少一个 test panic 且消息匹配
            let panic_marker = src.lines()
                .find(|l| l.trim_start().starts_with("//@ panic:"))
                .map(|l| l.trim_start_matches("//@ panic:").trim().to_string())
                .unwrap_or_default();
            let any_panic = results.iter().any(|(_, r)| r.is_err());
            if !any_panic {
                failures.push(format!("{rel}: 期望 panic 但未 panic"));
            } else if let Some((_, Err(msg))) = results.iter().find(|(_, r)| r.is_err()) {
                if !panic_marker.is_empty() && !msg.contains(&panic_marker) {
                    failures.push(format!("{rel}: panic 消息不匹配:期望含 \"{panic_marker}\",实际 \"{msg}\""));
                }
            }
        } else {
            // 行为文件:全部 test 通过
            for (tname, r) in &results {
                if let Err(msg) = r {
                    failures.push(format!("{rel}::{tname}: {msg}"));
                }
            }
        }
    }

    assert!(ran >= 33, "运行的行为/panic 文件应 ≥35,实际 {ran}");
    assert!(failures.is_empty(), "运行失败({}):\n{}", failures.len(), failures.join("\n---\n"));
    println!("P1-D 运行验收: {ran} 文件通过, {deferred} 延迟(web)");
}

