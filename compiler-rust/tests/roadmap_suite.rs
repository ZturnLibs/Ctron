//! R 线路线图锚点套件:tests/roadmap/ 语料(测试先行,红 = 规范锚)。
//!
//! 设计与翻转协议:docs/superpowers/plans/2026-09-07-r-tests-design.md
//! 每行 = (文件, 里程碑, 当前锚定状态)。断言语义的"今天":
//!   - Green:解释器全 test 通过(语义回归锚/fmt 夹具,须永久保持);
//!   - CheckRed:check 必须失败且诊断码 ⊇ 列出码(钉住"红 = 未实现",不是语料笔误);
//!   - NegRed:neg 锚,check 失败且不得含目标码(目标码出现 = 实现落地 → 按翻转协议迁移);
//!   - PanicMsgRed:运行 panic 但消息不含 marker(R-P4b 翻转判据)。
//! 里程碑落地时:文件登记进 run/check/native 主套件 → 删除本表对应行 → 打印计数递减。

use std::path::{Path, PathBuf};

enum Status {
    /// 今天就应全绿:check 过 + 解释器全 test 通过(语义回归锚/fmt 夹具/已兑现特性)
    Green,
    /// 运行时未实现锚:check 可过,但解释器必须失败(R-P2a/R-P2b/R-P3b 域)
    RunRed,
    /// 解析/检查未实现锚:check 必须失败(R-P3c 域)
    CheckRed,
    /// neg 锚(未生效):目标码未产出即通过;目标码出现 = 实现落地 → 按翻转协议迁移
    NegPending(&'static str),
    /// neg 已生效:目标码必须产出(检查层先行落地;里程碑完成时迁入 check_suite 期望表)
    NegGreen(&'static str),
    /// panic 锚:今天运行 panic,但消息不含 marker(R-P4b 翻转判据)
    PanicMsgRed(&'static str),
}

fn anchors() -> Vec<(&'static str, &'static str, Status)> {
    vec![
        // ---- R-P2a 容器(检查器已注册类型;运行时未实现) ----
        ("r2a_list.ct", "R-P2a", Status::RunRed),
        ("r2a_list_oob.panic.ct", "R-P2a", Status::RunRed),
        ("r2a_map.ct", "R-P2a", Status::RunRed),
        ("r2a_set.ct", "R-P2a", Status::RunRed),
        ("r2a_sb.ct", "R-P2a", Status::RunRed),
        ("r2a_container_send.ct", "R-P2a", Status::RunRed),
        ("r2a_container_alloc.neg.ct", "R-P2a", Status::NegGreen("E3040")),
        // ---- R-P2b std 能力(std.* 桩已解析;运行时未实现) ----
        ("r2b_fs_fake.ct", "R-P2b", Status::RunRed),
        ("r2b_time.ct", "R-P2b", Status::RunRed),
        ("r2b_env.ct", "R-P2b", Status::RunRed),
        // ---- R-P3 闭包/迭代器/模式 ----
        ("r3a_capture.ct", "R-P3a", Status::Green),
        ("r3a_capture_var.neg.ct", "R-P3a", Status::NegPending("E3070")),
        ("r3b_adapters.ct", "R-P3b", Status::RunRed),
        ("r3b_iter_trait.ct", "R-P3b", Status::RunRed),
        ("r3c_guards.ct", "R-P3c", Status::CheckRed),
        ("r3c_guard_exhaustive.neg.ct", "R-P3c", Status::NegPending("E2030")),
        // ---- R-P2d fmt 夹具(行为测试身份,今天绿) ----
        ("r2d_fmt_fixture.ct", "R-P2d", Status::Green),
        ("r2d_fmt_chain.ct", "R-P2d", Status::Green),
        // ---- R-P4 ----
        ("r4b_panic_location.panic.ct", "R-P4b", Status::PanicMsgRed("r4b_panic_location.ct")),
        ("r4c_arena_send.neg.ct", "R-P4c", Status::NegGreen("E3010")),
        // ---- R-P5 comptime ----
        ("r5a_const_size.ct", "R-P5a", Status::Green),
        ("r5a_semantics.ct", "R-P5a", Status::Green),
    ]
}

fn roadmap_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests/roadmap")
}

fn check_diags(src: &str) -> Vec<ctron::token::Diagnostic> {
    ctron::check_src(src, ctron::sem::Profile::Full)
}

#[test]
fn roadmap_anchors_match_design() {
    let dir = roadmap_dir();
    let mut failures: Vec<String> = Vec::new();
    let mut green = 0usize;
    let mut anchored = 0usize;

    for (name, _milestone, status) in anchors() {
        let path = dir.join(name);
        let Ok(src) = std::fs::read_to_string(&path) else {
            failures.push(format!("{name}: 语料文件缺失"));
            continue;
        };
        match status {
            Status::Green => {
                let results = ctron::interp::run_test_file(&src, ctron::sem::Profile::Full);
                if results.is_empty() {
                    failures.push(format!("{name}: Green 锚未产出任何 test 结果(check 未过?)"));
                }
                for (t, r) in &results {
                    if let Err(msg) = r {
                        failures.push(format!("{name}::{t}: Green 锚运行失败: {msg}"));
                    }
                }
                green += 1;
            }
            Status::RunRed => {
                let results = ctron::interp::run_test_file(&src, ctron::sem::Profile::Full);
                if results.iter().all(|(_, r)| r.is_ok()) && !results.is_empty() {
                    failures.push(format!(
                        "{name}: RunRed 锚今天运行全过了 —— 实现已落地?请按翻转协议迁移到主套件"
                    ));
                }
                anchored += 1;
            }
            Status::CheckRed => {
                let diags = check_diags(&src);
                if diags.is_empty() {
                    failures.push(format!(
                        "{name}: CheckRed 锚今天 check 通过了 —— 实现已落地?请按翻转协议迁移到主套件"
                    ));
                }
                anchored += 1;
            }
            Status::NegPending(target) => {
                let diags = check_diags(&src);
                let codes: Vec<&str> = diags.iter().map(|d| d.code).collect();
                if codes.contains(&target) {
                    failures.push(format!(
                        "{name}: 目标码 {target} 已产出 —— 实现已落地?请按翻转协议迁移到 check_suite 期望表"
                    ));
                }
                anchored += 1;
            }
            Status::NegGreen(target) => {
                let diags = check_diags(&src);
                let codes: Vec<&str> = diags.iter().map(|d| d.code).collect();
                if !codes.contains(&target) {
                    failures.push(format!("{name}: NegGreen 锚期望 {target},实际 {codes:?}"));
                }
                anchored += 1;
            }
            Status::PanicMsgRed(marker) => {
                let results = ctron::interp::run_test_file(&src, ctron::sem::Profile::Full);
                match results.iter().find(|(_, r)| r.is_err()) {
                    Some((_, Err(msg))) => {
                        if msg.contains(marker) {
                            failures.push(format!(
                                "{name}: panic 消息已含源位置(\"{marker}\") —— R-P4b 已落地?请迁移到主套件"
                            ));
                        }
                    }
                    _ => failures.push(format!("{name}: PanicMsgRed 锚未按预期 panic")),
                }
                anchored += 1;
            }
        }
    }

    // 语料卫生:roadmap/ 根下不得有未登记的 .ct 文件
    let mut stray = Vec::new();
    if let Ok(entries) = std::fs::read_dir(&dir) {
        for e in entries.flatten() {
            let p = e.path();
            if p.is_file() && p.extension().is_some_and(|x| x == "ct") {
                let name = p.file_name().unwrap().to_string_lossy().to_string();
                if !anchors().iter().any(|(n, _, _)| *n == name.as_str()) {
                    stray.push(name);
                }
            }
        }
    }
    if !stray.is_empty() {
        failures.push(format!("roadmap/ 存在未登记锚点文件: {stray:?}(加入 anchors() 表)"));
    }

    println!("路线图锚点进度: 绿(Green) {green} / 锚(红) {anchored} / 共 {}", green + anchored);
    for (name, milestone, _) in &anchors() {
        let state = if failures.iter().any(|f| f.starts_with(name)) { "✗" } else { "✓" };
        println!("  {state} {milestone:6} {name}");
    }
    assert!(failures.is_empty(), "锚点状态与设计不符({}):\n{}", failures.len(), failures.join("\n"));
}

#[test]
fn roadmap_module_cases() {
    let root = roadmap_dir().join("modules");
    let mut failures: Vec<String> = Vec::new();

    // caps_fs:今天即应产出 E4010(E4010 机制已实现,caps 先例同型)
    {
        let dir = root.join("caps_fs");
        let ctcl = std::fs::read_to_string(dir.join("Ctron.ctcl")).unwrap_or_default();
        let (manifest, _mdiags) = ctron::check::parse_manifest(&ctcl);
        let mut cts = Vec::new();
        collect_ct(&dir, &mut cts);
        let files: Vec<(String, String)> = cts.iter()
            .map(|c| (format!("capsfs.{}", c.file_stem().unwrap().to_string_lossy()), std::fs::read_to_string(c).unwrap()))
            .collect();
        let diags = ctron::check_package(&files, Some(manifest), ctron::sem::Profile::Full);
        let codes: Vec<&str> = diags.iter().flat_map(|(_, v)| v.iter()).map(|d| d.code).collect();
        if !codes.contains(&"E4010") {
            failures.push(format!("caps_fs: 期望 E4010,实际 {codes:?}"));
        }
    }

    // path_dep/app:跨包解析未实现(R-P7 锚)—— 必须以 E2020 失败
    {
        let dir = root.join("path_dep/app");
        let ctcl = std::fs::read_to_string(dir.join("Ctron.ctcl")).unwrap_or_default();
        let (manifest, _mdiags) = ctron::check::parse_manifest(&ctcl);
        let mut cts = Vec::new();
        collect_ct(&dir, &mut cts);
        let files: Vec<(String, String)> = cts.iter()
            .map(|c| (format!("pathapp.{}", c.file_stem().unwrap().to_string_lossy()), std::fs::read_to_string(c).unwrap()))
            .collect();
        let diags = ctron::check_package(&files, Some(manifest), ctron::sem::Profile::Full);
        let codes: Vec<&str> = diags.iter().flat_map(|(_, v)| v.iter()).map(|d| d.code).collect();
        if codes.contains(&"E2020") {
            // 锚定状态:跨包解析落地后此断言反转(迁移为行为用例)
        } else {
            failures.push(format!("path_dep/app: 期望以 E2020 失败(R-P7 锚),实际 {codes:?}"));
        }
    }

    // path_dep/lib:被依赖包自身必须零诊断(今天与将来都成立)
    {
        let dir = root.join("path_dep/lib");
        let ctcl = std::fs::read_to_string(dir.join("Ctron.ctcl")).unwrap_or_default();
        let (manifest, _mdiags) = ctron::check::parse_manifest(&ctcl);
        let mut cts = Vec::new();
        collect_ct(&dir, &mut cts);
        let files: Vec<(String, String)> = cts.iter()
            .map(|c| (format!("libmath.{}", c.file_stem().unwrap().to_string_lossy()), std::fs::read_to_string(c).unwrap()))
            .collect();
        let diags = ctron::check_package(&files, Some(manifest), ctron::sem::Profile::Full);
        let all: Vec<&ctron::token::Diagnostic> = diags.iter().flat_map(|(_, v)| v.iter()).collect();
        if !all.is_empty() {
            let detail = all.iter().map(|d| format!("{}:{}", d.code, d.span.line)).collect::<Vec<_>>().join(" ");
            failures.push(format!("path_dep/lib: 应零诊断,实际 {detail}"));
        }
    }

    assert!(failures.is_empty(), "roadmap modules 用例与设计不符:\n{}", failures.join("\n"));
}

fn collect_ct(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if p.is_dir() {
            collect_ct(&p, out);
        } else if p.extension().is_some_and(|x| x == "ct") {
            out.push(p);
        }
    }
}
