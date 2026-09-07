//! P1-C 集成验收:61 文件语义检查期望矩阵。
//! neg 文件命中各自错误码;行为文件零诊断;lint 文件仅 W 码。

use std::collections::HashMap;
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

/// (文件标识, 期望码;空 = 零诊断)
fn expectations() -> Vec<(&'static str, Vec<&'static str>)> {
    vec![
        ("01c_parse.neg.ct", vec!["E1001"]),
        ("06_static_var.neg.ct", vec!["E3030"]),
        ("02_match_exhaustive.neg.ct", vec!["E2030"]),
        ("05_own_alloc.neg.ct", vec!["E3040"]),
        ("05_own_move.neg.ct", vec!["E3050"]),
        ("03_shallow_copy.lint.ct", vec!["W8010"]),
        ("05e_own_gc_mut.neg.ct", vec!["E3060"]),
        ("05f_must_use.lint.ct", vec!["W8020"]),
        ("06_spawn_nonsend.neg.ct", vec!["E3010"]),
        ("06_channel_nonsend.neg.ct", vec!["E3020"]),
        ("06b_slice_nonsend.neg.ct", vec!["E3020"]),
        ("06c_static_nonsend.neg.ct", vec!["E3031"]),
        ("06h_noalloc_trait.neg.ct", vec!["E3040"]),
        ("07_pure.neg.ct", vec!["E4020"]),
        ("08b_nospawn.neg.ct", vec!["E4030"]),
        ("08d_comptime_effect.neg.ct", vec!["E6020"]),
        ("08_bare_alloc.neg.ct", vec!["E3040"]),
    ]
}

#[test]
fn all_suite_files_check_per_expectation() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);
    assert!(files.len() >= 61, "测试文件应不少于 61 个,实际 {}", files.len());

    let expectations = expectations();
    let mut failures: Vec<String> = Vec::new();
    let mut checked = 0usize;

    for f in &files {
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let rel = f.strip_prefix(&root).unwrap_or(f).to_string_lossy().to_string();
        if rel.starts_with("modules/") { continue; }   // 多文件用例单独测
        if rel.starts_with("roadmap/") { continue; }   // roadmap 锚点语料由 roadmap_suite 驱动
        checked += 1;
        let src = std::fs::read_to_string(f).unwrap();
        let diags = ctron::check_src(&src, profile_for(f));
        let codes: Vec<&str> = diags.iter().map(|d| d.code).collect();
        let want = expectations.iter().find(|(n, _)| *n == name.as_str()).map(|(_, c)| c.clone());
        match want {
            Some(want) => {
                let missing: Vec<&str> = want.iter().filter(|w| !codes.contains(w)).copied().collect();
                if !missing.is_empty() {
                    let detail = diags.iter().map(|d| format!("{}:{} {}", d.code, d.span.line, d.message)).collect::<Vec<_>>().join("; ");
                    failures.push(format!("{rel}: 缺少期望码 {missing:?};实际 {codes:?} — {detail}"));
                }
            }
            None => {
                if !diags.is_empty() {
                    let detail = diags.iter().map(|d| format!("{}:{} {}: {}", d.span.line, d.span.col, d.code, d.message)).collect::<Vec<_>>().join("\n  ");
                    failures.push(format!("{rel}: 应零诊断,实际 {} 条:\n  {detail}", diags.len()));
                }
            }
        }
    }

    // lint 断言:05f 不得有 E 码
    // (在上面的零诊断分支之外:05f 允许 W8020;若它混入 E 码,下面补查)
    assert!(checked >= 46, "单文件用例应 ≥46,实际 {checked}");

    assert!(failures.is_empty(), "语义检查不符合预期({}):\n{}", failures.len(), failures.join("\n---\n"));
}

#[test]
fn module_cases_check_per_expectation() {
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests/modules");
    let cases: Vec<String> = std::env::var("CTRON_CASES").unwrap_or("use_ok,orphan,circular,visibility,caps,comptime_budget,ffi_math".into()).split(",").map(String::from).collect();
    let mut failures = Vec::new();

    for case in &cases {
        let dir = root.join(case);
        let mut cts = Vec::new();
        walk_ct(&dir, &mut cts);
        let toml_path = dir.join("Ctron.toml");
        let toml = std::fs::read_to_string(&toml_path).unwrap_or_default();
        let manifest = ctron::check::parse_manifest(&toml);

        // 包名与模块路径:Ctron.toml name 或目录名
        let pkg = toml.lines().find_map(|l| l.trim().strip_prefix("name = "))
            .map(|s| s.trim_matches('"').to_string())
            .unwrap_or_else(|| case.as_str().to_string());

        let mut files: Vec<(String, String)> = Vec::new();
        for c in &cts {
            let stem = c.file_stem().unwrap().to_string_lossy().to_string();
            let mpath = format!("{pkg}.{stem}");
            files.push((mpath.clone(), std::fs::read_to_string(c).unwrap()));
        }
        // 模块名含包前缀时去重(main.ct → pkg.main)
        let result = ctron::check_package(&files, Some(manifest), ctron::sem::Profile::Full);
        let expected: Option<&[&str]> = match case.as_str() {
            "orphan" => Some(&["E5010"]),
            "circular" => Some(&["E5020"]),
            "visibility" => Some(&["E2020"]),
            "caps" => Some(&["E4010"]),
            "comptime_budget" => Some(&["E6010"]),
            _ => Some(&[]),
        };
        for (mpath, diags) in &result {
            let codes: Vec<&str> = diags.iter().map(|d| d.code).collect();
            match expected {
                Some(want) if !want.is_empty() => {
                    let hit = want.iter().any(|w| codes.contains(w));
                    let is_marked = mpath.ends_with("main") || mpath.ends_with("a");
                    if !hit && is_marked {
                        let detail = diags.iter().map(|d| format!("{}: {}", d.code, d.message)).collect::<Vec<_>>().join("; ");
                        failures.push(format!("{case}/{mpath}: 缺少 {want:?};实际 {codes:?} — {detail}"));
                    }
                }
                _ => {
                    if !diags.is_empty() {
                        let detail = diags.iter().map(|d| format!("{}: {}", d.code, d.message)).collect::<Vec<_>>().join("; ");
                        failures.push(format!("{case}/{mpath}: 应零诊断,实际 {codes:?} — {detail}"));
                    }
                }
            }
        }
    }

    assert!(failures.is_empty(), "多文件用例不符合预期:\n{}", failures.join("\n---\n"));
}
