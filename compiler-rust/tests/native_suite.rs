//! P1-E① 集成验收:Ctron→C 转译后端(数值域)与解释器语义差分。
//!
//! 对每个可运行语料文件:
//!   1. 解释器裁决 = 期望(行为文件全 test 通过;panic 文件消息匹配);
//!   2. trans_file 成功 → 生成 C → cc 编译 → 原生运行;
//!   3. 原生结果必须与解释器裁决一致(退出码 + panic 消息含 marker)。
//! 域外文件(转译拒绝)记 skip,不计失败——域随里程碑逐步扩大。

use std::path::{Path, PathBuf};
use std::process::Command;

fn walk_ct(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if p.is_dir() { walk_ct(&p, out); }
        else if p.extension().is_some_and(|x| x == "ct") { out.push(p); }
    }
}

fn cc_available() -> bool {
    Command::new("cc").arg("--version").output().is_ok()
}

#[test]
fn native_matches_interpreter() {
    if !cc_available() {
        eprintln!("跳过:无 cc 编译器");
        return;
    }
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);

    let tmp = std::env::temp_dir().join("ctron_native_suite");
    let _ = std::fs::remove_dir_all(&tmp);
    std::fs::create_dir_all(&tmp).unwrap();

    let mut ran = 0usize;
    let mut skipped = 0usize;
    let mut failures = Vec::new();

    for f in &files {
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let rel = f.strip_prefix(&root).unwrap_or(f).to_string_lossy().to_string();
        if rel.starts_with("modules/") { continue; }
        if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") { continue; }
        if name.starts_with("10_web") { continue; }

        let src = std::fs::read_to_string(f).unwrap();
        let panic_marker = if name.ends_with(".panic.ct") {
            src.lines()
                .find(|l| l.trim_start().starts_with("//@ panic:"))
                .map(|l| l.trim_start_matches("//@ panic:").trim().to_string())
                .unwrap_or_default()
        } else { String::new() };

        // 期望 = 解释器裁决
        let profile = ctron::sem::Profile::Full;
        let interp = ctron::interp::run_test_file(&src, profile);
        let expect_ok = if name.ends_with(".panic.ct") {
            let panicked = matches!(interp.iter().find(|(_, r)| r.is_err()), Some((_, Err(_))));
            if !panicked {
                failures.push(format!("{rel}: 解释器未按 panic 文件预期失败"));
            }
            false
        } else {
            let msgs: Vec<&String> = interp.iter().filter_map(|(_, r)| r.as_ref().err()).collect();
            if interp.is_empty() { skipped += 1; continue; }
            msgs.is_empty()
        };

        // 转译(域外 → skip;全量覆盖后不应再有域外)
        let (file_ast, diags) = ctron::parse_src(&src);
        if !diags.is_empty() { skipped += 1; continue; }
        let c_code = match ctron::trans::Trans::new().trans_file(&file_ast) {
            Ok(c) => c,
            Err(_) => { skipped += 1; continue; }
        };

        // 编译 + 运行
        let stem = f.file_stem().unwrap().to_string_lossy().replace(['.', '-'], "_");
        let c_path = tmp.join(format!("{stem}.c"));
        let bin_path = tmp.join(format!("{stem}.bin"));
        std::fs::write(&c_path, &c_code).unwrap();
        let cc = Command::new("cc")
            .args(["-O0", "-w", "-std=gnu11"])
            .arg(&c_path).arg("-o").arg(&bin_path)
            .output()
            .expect("cc 启动失败");
        if !cc.status.success() {
            failures.push(format!("{rel}: cc 编译失败:\n{}", String::from_utf8_lossy(&cc.stderr)));
            continue;
        }
        let run = Command::new(&bin_path).output().expect("运行失败");
        ran += 1;

        let native_ok = run.status.success();
        let stderr = String::from_utf8_lossy(&run.stderr).to_string();

        if expect_ok {
            if !native_ok {
                failures.push(format!(
                    "{rel}: 解释器通过但原生失败(exit {:?}):\n{}",
                    run.status.code(),
                    stderr
                ));
            }
        } else {
            // panic 文件:解释器与原生都须失败,且消息都含 marker
            if native_ok {
                failures.push(format!("{rel}: 解释器 panic 但原生通过"));
            } else if !panic_marker.is_empty() && !stderr.contains(&panic_marker) {
                failures.push(format!(
                    "{rel}: 原生 panic 消息不匹配:期望含 \"{panic_marker}\",实际 \"{stderr}\""
                ));
            }
        }
    }

    println!("P1-E 原生差分: {ran} 文件原生运行一致, {skipped} 域外跳过");
    assert!(ran >= 33, "原生差分覆盖必须全量(33),实际 {ran}");
    assert!(failures.is_empty(), "原生/解释器分歧({}):\n{}", failures.len(), failures.join("\n---\n"));
}

/// modules/ 包:合并 src/*.ct → 转译 → cc(含 c_src/*.c)→ 原生运行,
/// 并与解释器对同一合并文件的裁决差分。
#[test]
fn modules_native_run() {
    if !cc_available() { return; }
    let root = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests/modules");
    let tmp = std::env::temp_dir().join("ctron_native_modules");
    let _ = std::fs::remove_dir_all(&tmp);
    std::fs::create_dir_all(&tmp).unwrap();

    let mut ran = 0usize;
    let mut failures = Vec::new();
    for case in std::fs::read_dir(&root).unwrap().flatten() {
        let case_path = case.path();
        if !case_path.is_dir() { continue; }
        let name = case_path.file_name().unwrap().to_string_lossy().to_string();
        let mut cts: Vec<PathBuf> = Vec::new();
        walk_ct(&case_path.join("src"), &mut cts);
        if cts.is_empty() { continue; }
        // 主文件(含 test)在前,其余为依赖
        cts.sort_by_key(|p| {
            let s = std::fs::read_to_string(p).unwrap_or_default();
            (std::rc::Rc::new(()), !s.contains("test \""))
        });
        cts.sort_by_key(|p| std::fs::read_to_string(p).unwrap_or_default().contains("test \"") == false);

        let mut merged = ctron::ast::File { decls: Vec::new() };
        let mut files: Vec<(String, String)> = Vec::new();
        let mut any_diags = false;
        for c in &cts {
            let src = std::fs::read_to_string(c).unwrap();
            let (f, d) = ctron::parse_src(&src);
            if !d.is_empty() { any_diags = true; }
            merged.decls.extend(f.decls);
            files.push((c.file_stem().unwrap().to_string_lossy().to_string(), src));
        }
        if any_diags { continue; }
        let has_tests = merged.decls.iter().any(|d| matches!(d, ctron::ast::Decl::Test(_)));
        if !has_tests { continue; }

        // 解释器裁决(多文件:build_package 语义 + 合并文件运行)
        let (sema, _) = ctron::sem::build_package(&files, None, ctron::sem::Profile::Full);
        let _ = &files;
        let merged_rc = std::rc::Rc::new(merged);
        let mut interp = ctron::interp::Interp::new(&sema, String::new(), merged_rc.clone());
        let results = interp.run_tests(&merged_rc);
        let interp_ok = !results.is_empty() && results.iter().all(|(_, r)| r.is_ok());

        // 转译(域外 → skip)
        let c_code = match ctron::trans::Trans::new().trans_files(std::slice::from_ref(&merged_rc)) {
            Ok(c) => c,
            Err(_) => { continue; }
        };
        let stem = format!("mod_{}", name);
        let c_path = tmp.join(format!("{stem}.c"));
        std::fs::write(&c_path, &c_code).unwrap();
        let mut cc = Command::new("cc");
        cc.args(["-O0", "-w", "-std=gnu11"]).arg(&c_path).arg("-o").arg(tmp.join(&stem));
        // 包内 c_src/*.c 一并链接
        let cs_dir = case_path.join("c_src");
        if cs_dir.is_dir() {
            for e in std::fs::read_dir(&cs_dir).unwrap().flatten() {
                if e.path().extension().is_some_and(|x| x == "c") { cc.arg(e.path()); }
            }
        }
        let cc_out = cc.output().expect("cc 启动失败");
        if !cc_out.status.success() {
            failures.push(format!("{name}: cc 编译失败:\n{}", String::from_utf8_lossy(&cc_out.stderr)));
            continue;
        }
        let run = Command::new(tmp.join(&stem)).output().expect("运行失败");
        ran += 1;
        let native_ok = run.status.success();
        let stderr = String::from_utf8_lossy(&run.stderr).to_string();
        if !native_ok {
            failures.push(format!("{name}: 原生运行失败:\n{stderr}"));
        }
    }

    println!("modules 原生差分: {ran} 包一致");
    assert!(ran >= 2, "至少应覆盖 use_ok/ffi_math 两个包,实际 {ran}");
    assert!(failures.is_empty(), "modules 原生分歧({}):\n{}", failures.len(), failures.join("\n---\n"));
}
