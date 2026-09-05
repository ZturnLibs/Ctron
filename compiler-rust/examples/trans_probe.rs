use std::path::{Path, PathBuf};
fn walk(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(es) = std::fs::read_dir(dir) else { return };
    for e in es.flatten() {
        let p = e.path();
        if p.is_dir() { walk(&p, out); }
        else if p.extension().is_some_and(|x| x == "ct") { out.push(p); }
    }
}
fn main() {
    let mut files = Vec::new();
    walk(Path::new("../tests"), &mut files);
    files.sort();
    for f in files {
        let name = f.file_name().unwrap().to_string_lossy().to_string();
        if name.ends_with(".neg.ct") || name.ends_with(".lint.ct") { continue; }
        if name.starts_with("10_web") { continue; }
        let src = std::fs::read_to_string(&f).unwrap();
        let (fa, diags) = ctron::parse_src(&src);
        if !diags.is_empty() { continue; }
        match ctron::trans::Trans::new().trans_file(&fa) {
            Ok(_) => println!("OK   {}", name),
            Err(r) => println!("SKIP {}: {}", name, r),
        }
    }
}
