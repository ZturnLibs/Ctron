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

        // 转译(域外 → skip)
        let (file_ast, diags) = ctron::parse_src(&src);
        if !diags.is_empty() { skipped += 1; continue; }
        let c_code = match ctron::trans::Trans::new().trans_file(&file_ast) {
            Ok(c) => c,
            Err(_reason) => { skipped += 1; continue; }
        };

        // 编译 + 运行
        let stem = f.file_stem().unwrap().to_string_lossy().replace(['.', '-'], "_");
        let c_path = tmp.join(format!("{stem}.c"));
        let bin_path = tmp.join(format!("{stem}.bin"));
        std::fs::write(&c_path, &c_code).unwrap();
        let cc = Command::new("cc")
            .args(["-O0", "-std=gnu11"])
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

    println!("P1-E① 原生差分: {ran} 文件原生运行一致, {skipped} 域外跳过");
    assert!(ran >= 3, "数值域至少应覆盖 3 个语料文件,实际 {ran}");
    assert!(failures.is_empty(), "原生/解释器分歧({}):\n{}", failures.len(), failures.join("\n---\n"));
}
