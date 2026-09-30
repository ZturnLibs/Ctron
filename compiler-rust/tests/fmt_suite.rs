//! R-P2d 集成验收:`ctron fmt` —— 规范格式逐规则 + 全语料三断言(幂等/AST 等价/行为矩阵)。
//! 设计依据 docs/superpowers/plans/2026-09-12-cli-test-fmt.md 与 docs/fmt-spec.md。

use ctron::fmt::fmt_src;

fn fmt_ok(src: &str) -> String {
    match fmt_src(src) {
        Ok(s) => s,
        Err(e) => panic!("fmt 失败: {e}\n源码:\n{src}"),
    }
}

// ---------- 逐规则 ----------

#[test]
fn binary_ops_get_spaces_assignment_aligned() {
    assert_eq!(fmt_ok("let x=1+2\n"), "let x = 1 + 2\n");
    assert_eq!(fmt_ok("let b=a==1&&c>2\n"), "let b = a == 1 && c > 2\n");
}

#[test]
fn call_and_colon_spacing() {
    assert_eq!(fmt_ok("fn f(x:I32,y:Str)->I32{\nreturn x\n}\n"),
        "fn f(x: I32, y: Str) -> I32 {\n    return x\n}\n");
}

#[test]
fn indent_is_four_spaces_per_block_level() {
    let src = "fn f() {\nif x {\nreturn 1\n}\n}\n";
    let want = "fn f() {\n    if x {\n        return 1\n    }\n}\n";
    assert_eq!(fmt_ok(src), want);
}

#[test]
fn empty_block_stays_tight() {
    assert_eq!(fmt_ok("fn f() {}\n"), "fn f() {}\n");
}

#[test]
fn else_joins_closing_brace_line() {
    let src = "fn f() {\n    if x {\n        return 1\n    }\n    else {\n        return 2\n    }\n}\n";
    let want = "fn f() {\n    if x {\n        return 1\n    } else {\n        return 2\n    }\n}\n";
    assert_eq!(fmt_ok(src), want);
}

#[test]
fn inline_if_expression_kept_on_one_line() {
    let src = "fn f(n: I32) -> Str {\n    let label = if n > 0 { \"pos\" } else { \"neg\" }\n    return label\n}\n";
    assert_eq!(fmt_ok(src), src, "单行 if 表达式块不得被拆行");
}

#[test]
fn comments_preserved_own_line_and_trailing() {
    let src = "// 头注释\nfn f() {\n    // 独占行注释\n    let x = 1 // 行尾注释\n    return x\n}\n";
    assert_eq!(fmt_ok(src), src, "注释必须逐字保留且位置分类不变");
}

#[test]
fn panic_marker_comment_preserved_verbatim() {
    let src = "//@ panic: overflow\n\ntest \"explodes\" {\n    let m: U8 = 255\n    let _ = m + 1u8\n}\n";
    assert_eq!(fmt_ok(src), src, "//@ 标记是 campaign/测试语义,必须逐字保留");
}

#[test]
fn string_literals_verbatim_including_interpolation() {
    let src = "fn greet(name: Str, n: I32) -> Str {\n    return \"hi {name} x{n} // not a comment\"\n}\n";
    assert_eq!(fmt_ok(src), src, "字符串字面量按源切片逐字输出,插值与内部 // 不得改动");
}

#[test]
fn number_literals_verbatim_including_suffix_and_base() {
    let src = "test \"nums\" {\n    assert_eq(255u8 +% 1u8, 0u8)\n    assert_eq(0xFF, 255)\n    assert_eq(2.5, 2.5)\n}\n";
    assert_eq!(fmt_ok(src), src);
}

#[test]
fn chain_break_preserved_and_idempotent() {
    let src = "fn f() -> I32 {\n    return xs\n        .map(g)\n        .filter(h)\n        .len()\n}\n";
    let once = fmt_ok(src);
    assert!(once.contains(".map(g)\n"), "链断行应保留:\n{once}");
    assert_eq!(fmt_ok(&once), once, "链断行必须幂等");
}

#[test]
fn single_line_chain_stays_joined() {
    let src = "fn f() -> I32 {\n    return xs.map(g).len()\n}\n";
    assert_eq!(fmt_ok(src), src);
}

#[test]
fn blank_lines_collapse_to_one() {
    let src = "fn a() {}\n\n\n\nfn b() {}\n";
    assert_eq!(fmt_ok(src), "fn a() {}\n\nfn b() {}\n");
}

#[test]
fn struct_literal_spacing() {
    let src = "fn f() -> Point {\n    return Point { x: 1, y: 2 }\n}\n";
    assert_eq!(fmt_ok(src), src);
}

#[test]
fn closure_pipe_spacing() {
    let src = "fn f() -> I32 {\n    return xs.map(|x|x*2).sum()\n}\n";
    let want = "fn f() -> I32 {\n    return xs.map(|x| x * 2).sum()\n}\n";
    assert_eq!(fmt_ok(src), want);
}

#[test]
fn attribute_and_derive_tight() {
    let src = "#[pure]\n@derive(Show, Eq)\nstruct P {\n    x: I32,\n    y: I32,\n}\n";
    assert_eq!(fmt_ok(src), src);
}

#[test]
fn unary_not_and_borrow_spacing_regression() {
    // 回归:前缀 ! 的紧贴只在其右侧;`return!(...)`/`&&!b` 是错误输出
    let src = "fn f(a: Bool, b: Bool) -> Bool {\n    return !(!a && !b)\n}\n";
    assert_eq!(fmt_ok(src), src);
    let src2 = "test \"t\" {\n    assert(true && !false)\n}\n";
    assert_eq!(fmt_ok(src2), src2);
}

#[test]
fn lex_diags_are_an_error() {
    let src = "let x = \"未闭合\n";
    assert!(fmt_src(src).is_err(), "词法诊断必须报错而非静默输出");
}

// ---------- R9/R10 use 组自动折行与导入排序 ----------

#[test]
fn use_group_over_width_wraps_with_trailing_comma() {
    let src = "use std.str.{alpha, beta, gamma, delta, epsilon, zeta, eta, theta, iota, kappa, lambda, mu, nu, xi, omicron}\n";
    let out = fmt_ok(src);
    assert!(out.starts_with("use std.str.{\n"), "超宽单行组应折行:\n{out}");
    assert!(out.contains("    zeta,\n}"), "自动折行应补尾逗号:\n{out}");
    assert_eq!(fmt_ok(&out), out, "折行形态必须幂等");
}

#[test]
fn use_group_under_width_stays_inline() {
    let src = "use std.fs.{write_dir, read_dir}\n";
    assert_eq!(fmt_ok(src), "use std.fs.{ read_dir, write_dir }\n");
}

#[test]
fn use_group_items_sorted_multiline_keeps_trailing_comma_choice() {
    // 单行组:项排序
    let out = fmt_ok("use app.util.{triple, double}\n");
    assert_eq!(out, "use app.util.{ double, triple }\n");
    // 多行组重排:每行一项,尾逗号按原样保留(区别于 R9 自动折行恒补)
    let multi = "use app.util.{\ntriple,\ndouble\n}\n";
    let out2 = fmt_ok(multi);
    assert_eq!(out2, "use app.util.{\n    double,\n    triple\n}\n");
    assert_eq!(fmt_ok(&out2), out2, "多行重排形态必须幂等");
    // 原有尾逗号保留
    let out3 = fmt_ok("use app.util.{\ntriple,\ndouble,\n}\n");
    assert_eq!(out3, "use app.util.{\n    double,\n    triple,\n}\n");
}

#[test]
fn use_alias_sorts_by_original_name() {
    let out = fmt_ok("use app.util.{widget as w, gadget}\n");
    assert_eq!(out, "use app.util.{ gadget, widget as w }\n");
}

#[test]
fn use_ungrouped_alias_absorbed_into_statement() {
    // 回归:无组 use 的尾随别名必须并入语句 span,否则排序后 `as Alias` 悬空
    let src = "use b.mod\nuse a.sym as aliased\n";
    assert_eq!(fmt_ok(src), "use a.sym as aliased\nuse b.mod\n");
    let src2 = "use a.sym as aliased\nuse b.mod\n";
    assert_eq!(fmt_ok(src2), src2, "已排序文件必须原样幂等");
}

#[test]
fn use_statements_sort_within_run_blank_lines_split_runs() {
    let src = "use b.mod\nuse a.mod\n\nuse z.mod\nuse y.mod\n";
    let out = fmt_ok(src);
    assert_eq!(
        out,
        "use a.mod\nuse b.mod\n\nuse y.mod\nuse z.mod\n",
        "空行分段:段内排序,段间次序与空行保留"
    );
}

#[test]
fn use_comments_freeze_sort_and_rebuild() {
    // 语句间注释 → 分段且不排序
    let src = "use b.mod\n// keep\nuse a.mod\n";
    assert_eq!(fmt_ok(src), "use b.mod\n// keep\nuse a.mod\n");
    // 语句内注释 → 该语句冻结(不重排不折行)
    let src2 = "use app.util.{\ntriple, // t\ndouble,\n}\n";
    assert_eq!(
        fmt_ok(src2),
        "use app.util.{\n    triple, // t\n    double,\n}\n",
        "语句 span 含注释 → 冻结,只走 R1 排版"
    );
}

#[test]
fn use_ungrouped_long_path_not_wrapped() {
    let src = "use aaaa.bbbb.cccc.dddd.eeee.ffff.gggg.hhhh.iiii.jjjj.kkkk.llll.mmmm.nnnn.oooo\n";
    let out = fmt_ok(src);
    assert_eq!(out, src, "无组 use 不折行(R9)");
}

// ---------- 全语料三断言(roadmap 验收口径) ----------

/// R10 修订口径:use 声明序列与其组内项序列按多重集比对(imports 已拆全路径)
fn canon_file(f: &ctron::ast::File) -> (Vec<String>, Vec<String>) {
    let mut plain: Vec<String> = Vec::new();
    let mut uses: Vec<String> = Vec::new();
    for d in &f.decls {
        match d {
            ctron::ast::Decl::Use(u) => {
                let mut items: Vec<String> = u.imports.iter().map(|i| format!("{i:?}")).collect();
                items.sort();
                uses.push(items.join("\u{1}"));
            }
            other => plain.push(format!("{other:?}")),
        }
    }
    uses.sort();
    (plain, uses)
}

fn walk_ct(dir: &std::path::Path, out: &mut Vec<std::path::PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for e in entries.flatten() {
        let p = e.path();
        if p.is_dir() { walk_ct(&p, out); }
        else if p.extension().is_some_and(|x| x == "ct") { out.push(p); }
    }
}

#[test]
fn full_corpus_idempotent_and_parse_equal() {
    let root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);
    assert!(files.len() >= 61, "语料应至少 61 个 .ct");

    let mut failures = Vec::new();
    for f in &files {
        let rel = f.strip_prefix(&root).unwrap_or(f).to_string_lossy().to_string();
        // roadmap/ 是测试先行的红锚语料,可能尚未可解析,不入门禁
        if rel.starts_with("roadmap/") { continue; }
        // *.neg.ct 本就带词法级错误(判定面=//@ fail 码;lex_suite skip/check_suite
        // 表驱动同口径),fmt 不可达亦无意义,不入语料
        if rel.ends_with(".neg.ct") { continue; }
        let src = std::fs::read_to_string(f).unwrap();
        let formatted = match fmt_src(&src) {
            Ok(s) => s,
            Err(e) => { failures.push(format!("{rel}: fmt 失败: {e}")); continue; }
        };
        // 断言① 幂等:fmt(fmt(x)) == fmt(x)
        match fmt_src(&formatted) {
            Ok(again) if again == formatted => {}
            Ok(_) => failures.push(format!("{rel}: 不幂等")),
            Err(e) => failures.push(format!("{rel}: 二次 fmt 失败: {e}")),
        }
        // 断言② AST 等价:parse(fmt(x)) == parse(x)
        // use 声明按多重集比对(规范 R10:导入为集合语义,排序是规范行为;
        // 组内项同理),其余声明顺序仍严格比对
        let (before, bd) = ctron::parse_src(&src);
        let (after, ad) = ctron::parse_src(&formatted);
        if canon_file(&before) != canon_file(&after) || bd.len() != ad.len() {
            failures.push(format!("{rel}: fmt 前后 AST 不等"));
        }
    }
    assert!(failures.is_empty(), "{} 个语料失败:\n{}", failures.len(), failures.join("\n---\n"));
}

#[test]
fn full_corpus_behavior_matrix_unchanged() {
    let root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../tests");
    let mut files = Vec::new();
    walk_ct(&root, &mut files);

    let mut failures = Vec::new();
    let mut ran = 0usize;
    for f in &files {
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        let rel = f.strip_prefix(&root).unwrap_or(f).to_string_lossy().to_string();
        if rel.starts_with("roadmap/") || rel.starts_with("modules/") { continue; }
        if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") { continue; }
        if name.starts_with("10_web") { continue; }
        let profile = if name.starts_with("08_bare") { ctron::sem::Profile::Bare }
            else { ctron::sem::Profile::Full };
        let src = std::fs::read_to_string(f).unwrap();
        let formatted = fmt_ok(&src);
        let before = ctron::interp::run_test_file(&src, profile);
        let after = ctron::interp::run_test_file(&formatted, profile);
        // 结果对齐(测试名, Ok/Err 消息)逐项相等
        let norm = |rs: &[(String, Result<(), String>)]| -> Vec<(String, String)> {
            rs.iter().map(|(n, r)| (n.clone(), match r {
                Ok(()) => "ok".to_string(),
                Err(m) => format!("err:{m}"),
            })).collect()
        };
        if norm(&before) != norm(&after) {
            failures.push(format!("{rel}: fmt 前后行为矩阵变化"));
        }
        ran += 1;
    }
    // 顶层语料 55 个 .ct,扣除 neg/lint/web/roadmap/modules 后的行为文件约 36 个
    assert!(ran >= 30, "行为矩阵应覆盖 ≥30 个行为文件,实际 {ran}");
    assert!(failures.is_empty(), "{} 个行为差异:\n{}", failures.len(), failures.join("\n"));
}

// ---------- CLI 级:ctron fmt ----------

#[test]
fn cli_manifest_writeback_preserves_comments() {
    let base = std::env::temp_dir().join(format!("ctcl_wb_{}", std::process::id()));
    std::fs::create_dir_all(&base).unwrap();
    let p = base.join("Ctron.ctcl");
    let src = "// 头注释归 pkg\npkg {\n    name = \"z\"\n    // 格式版本,勿动\n    manifest_version = 1\n    version = \"0.1.0\"\n}\n";
    std::fs::write(&p, src).unwrap();
    let (_, _, code) = run_cli(&["manifest", p.to_str().unwrap(), "-w"]);
    assert_eq!(code, Some(0));
    let out = std::fs::read_to_string(&p).unwrap();
    assert_eq!(
        out,
        "// 头注释归 pkg\npkg {\n    // 格式版本,勿动\n    manifest_version = 1\n    name = \"z\"\n    version = \"0.1.0\"\n}\n"
    );
    let (sout2, _, code2) = run_cli(&["manifest", p.to_str().unwrap(), "-w"]);
    assert_eq!(code2, Some(0));
    assert!(sout2.contains("already canonical"), "sout={sout2:?}");
    std::fs::write(&p, "pkg {\n    name = \"z\"\n}\n").unwrap();
    let (_, _, code3) = run_cli(&["manifest", p.to_str().unwrap(), "-w"]);
    assert_eq!(code3, Some(1), "存在 E 级诊断拒绝写回");
    let _ = std::fs::remove_dir_all(&base);
}

#[test]
fn cli_manifest_add_dep_sorts_and_refuses_dup() {
    let base = std::env::temp_dir().join(format!("ctcl_add_{}", std::process::id()));
    std::fs::create_dir_all(&base).unwrap();
    let p = base.join("Ctron.ctcl");
    std::fs::write(
        &p,
        "pkg {\n    manifest_version = 1\n    name = \"app\"\n    version = \"0.1.0\"\n}\n\ndep \"zlib\" {\n    path = \"../zlib\"\n}\n",
    )
    .unwrap();
    let (_, _, code) = run_cli(&["manifest", p.to_str().unwrap(), "--add-dep=libmath=../lib"]);
    assert_eq!(code, Some(0));
    let out = std::fs::read_to_string(&p).unwrap();
    let i_lib = out.find("dep \"libmath\"").expect("libmath 应存在");
    let i_z = out.find("dep \"zlib\"").expect("zlib 应存在");
    assert!(i_lib < i_z, "新增依赖应按名排序:\n{out}");
    assert!(out.contains("path = \"../lib\""));
    let (_, _, code2) = run_cli(&["manifest", p.to_str().unwrap(), "--add-dep=libmath=../other"]);
    assert_eq!(code2, Some(1), "同名键控块禁止追加");
    let _ = std::fs::remove_dir_all(&base);
}

#[test]
fn cli_manifest_caps_and_remove_ops() {
    let base = std::env::temp_dir().join(format!("ctcl_ops_{}", std::process::id()));
    std::fs::create_dir_all(&base).unwrap();
    let p = base.join("Ctron.ctcl");
    std::fs::write(
        &p,
        "pkg {\n    manifest_version = 1\n    name = \"app\"\n    version = \"0.1.0\"\n    caps = [\"fs\"]\n}\n\ndep \"zlib\" {\n    path = \"../zlib\"\n}\n",
    )
    .unwrap();
    let (_, _, c1) = run_cli(&["manifest", p.to_str().unwrap(), "--add-cap=time"]);
    assert_eq!(c1, Some(0));
    let out = std::fs::read_to_string(&p).unwrap();
    assert!(out.contains("caps = [\"fs\", \"time\"]"), "\n{out}");
    let (_, _, c2) = run_cli(&["manifest", p.to_str().unwrap(), "--add-cap=net"]);
    assert_eq!(c2, Some(1));
    let (_, _, c3) = run_cli(&["manifest", p.to_str().unwrap(), "--remove-cap=fs"]);
    assert_eq!(c3, Some(0));
    let out = std::fs::read_to_string(&p).unwrap();
    assert!(out.contains("caps = [\"time\"]"), "\n{out}");
    let (_, _, c4) = run_cli(&["manifest", p.to_str().unwrap(), "--remove-dep=nope"]);
    assert_eq!(c4, Some(1));
    let (_, _, c5) = run_cli(&["manifest", p.to_str().unwrap(), "--remove-dep=zlib"]);
    assert_eq!(c5, Some(0));
    let out = std::fs::read_to_string(&p).unwrap();
    assert!(!out.contains("dep \"zlib\""), "\n{out}");
    let _ = std::fs::remove_dir_all(&base);
}

fn run_cli(args: &[&str]) -> (String, String, Option<i32>) {
    let out = std::process::Command::new(env!("CARGO_BIN_EXE_ctronr"))
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
    let path = std::env::temp_dir().join(format!("ctron_fmt_suite_{}_{}.ct", tag, std::process::id()));
    std::fs::write(&path, content).unwrap();
    path
}

#[test]
fn cli_fmt_prints_formatted_to_stdout() {
    let path = write_tempfile("print", "let x=1+2\n");
    let (stdout, _, code) = run_cli(&["fmt", path.to_str().unwrap()]);
    assert_eq!(code, Some(0));
    assert_eq!(stdout, "let x = 1 + 2\n");
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_fmt_check_reports_unformatted_and_w_fixes() {
    let path = write_tempfile("check", "let x=1+2\n");
    // --check:未格式化 → 非零退出且不改文件
    let (stdout, stderr, code) = run_cli(&["fmt", path.to_str().unwrap(), "--check"]);
    assert_eq!(code, Some(1));
    assert!(stdout.contains(path.to_str().unwrap()), "--check 应在 stdout 列出待格式化文件: {stdout}");
    assert!(stderr.contains("待格式化"));
    assert_eq!(std::fs::read_to_string(&path).unwrap(), "let x=1+2\n", "--check 不得改写文件");
    // -w:原位写回
    let (_, _, wcode) = run_cli(&["fmt", path.to_str().unwrap(), "-w"]);
    assert_eq!(wcode, Some(0));
    assert_eq!(std::fs::read_to_string(&path).unwrap(), "let x = 1 + 2\n");
    // 再 --check:已格式化 → 零退出
    let (_, _, ccode) = run_cli(&["fmt", path.to_str().unwrap(), "--check"]);
    assert_eq!(ccode, Some(0));
    let _ = std::fs::remove_file(&path);
}

#[test]
fn cli_fmt_pkg_dir_checks_all_sources() {
    let base = std::env::temp_dir().join(format!("ctron_fmt_pkg_{}", std::process::id()));
    let src_dir = base.join("src");
    std::fs::create_dir_all(&src_dir).unwrap();
    std::fs::write(base.join("Ctron.ctcl"), "pkg {\n    manifest_version = 1\n    name = \"mylib\"\n}\n").unwrap();
    std::fs::write(src_dir.join("a.ct"), "let a=1\n").unwrap();
    std::fs::write(src_dir.join("b.ct"), "let b = 2\n").unwrap();
    let (_, _, code) = run_cli(&["fmt", base.to_str().unwrap(), "--check"]);
    assert_eq!(code, Some(1), "a.ct 未格式化 → pkg --check 非零");
    let (_, _, wcode) = run_cli(&["fmt", base.to_str().unwrap(), "-w"]);
    assert_eq!(wcode, Some(0));
    assert_eq!(std::fs::read_to_string(src_dir.join("a.ct")).unwrap(), "let a = 1\n");
    let (_, _, ccode) = run_cli(&["fmt", base.to_str().unwrap(), "--check"]);
    assert_eq!(ccode, Some(0));
    let _ = std::fs::remove_dir_all(&base);
}
