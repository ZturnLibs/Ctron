#!/usr/bin/env python3
"""Ctron 测试集元检查。

在编译器存在之前,先保证测试集自身的格式、标记与错误码引用一致。
规则来源:tests/README.md §1–§4。测试集变更后运行:python3 tests/meta_check.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parent
sys.path.insert(0, str(ROOT.parent / "tools"))
import ctcl_check  # noqa: E402  (CTCL 清单 schema 校验,规范 config-language-v1 §5)

# 与 README §4 同步的错误码注册表
ERROR_CODES = {
    "E1001": "解析错误(通用语法违规;含比较不可链)",
    "E2010": "类型不匹配",
    "E2050": "bound 不满足(泛型实参不满足型参 bound)",
    "E2020": "未解析的名称",
    "E2030": "match 不穷尽",
    "E2040": "字面量超出期望整数类型宽度(§3.7;let/赋值/返回/实参四点)",
    "E2060": "无法推断类型实参(v0.7;请显式标注)",
    "E2061": "类型实参候选冲突(v0.7)",
    "E2070": "break/continue 出现在循环外(v0.7)",
    "E2071": "break/continue 越过带 Drop 局部的作用域(v0.7)",
    "E2072": "break/continue 穿越闭包边界(v0.7)",
    "E2080": "对不可变绑定赋值(let 局部;§4.0 v0.9)",
    "E3010": "spawn 捕获了非 Send 值",
    "E3020": "channel 收发非 Send 类型",
    "E3030": "static var 不存在",
    "E3031": "非 Send 类型作为全局/静态存储",
    "E3040": "no_alloc 上下文中出现 GC/String 分配",
    "E3050": "own 块内 move/borrow 违规",
    "E3060": "own 块内对 GC 值可变借用",
    "E3070": "闭包可变捕获未显式 Mutex[T] 包装(R 线 R-P3a)",
    "E4010": "能力使用超出 manifest 声明",
    "E4020": "#[pure] 函数含副作用",
    "E4030": "#[no_spawn] 上下文 spawn",
    "E4040": "#[trusted] 用于非 extern 声明(§9.6)",
    "E4041": "#[repr(c)] 用于非 struct 声明(§9.6 v0.6)",
    "E4042": "捕获闭包作 C-ABI 回调实参(无 env 槽;§9.6 v0.6)",
    "E4044": "变参形参(...)仅限 extern 声明(§9.6 v0.7)",
    "E4045": "裸引用形参拦截(v0.9·二;FFI 泳道)",
    "E4046": "Box 收口/裸指针能力面(FFI 泳道 14a8b73)",
    "E4047": "ptr_as_view 裸指针读侧信任门(FFI 泳道 c7018f3)",
    "E4050": "类直接持有需确定性释放的资源字段(§6.2)",
    "E5010": "trait 孤儿规则违规",
    "E5020": "循环依赖",
    "E5030": "use 导入同名 decl",
    "E5035": "use 别名与已有 decl 冲突(§2 包路径别名)",
    "E5054": "依赖工件摘要不符(闭源分发 D8-2 L2 记录-比对)",
    "E5055": "工件摘要未验(缺 self_digest/dep.digest;fail-closed 拒载)",
    "E5056": "轨迹复放不符(闭源分发 S3 信任协议;当前载体=ctron pkg verify --deep 工具面,加载期腿预留)",
    "E5057": "attest 与工件不符(闭源分发 S4-② 发布公证;artifact_digest/trace_count 漂移或伪造)",
    "E5060": "derive 插件未声明/插件包加载或接口面不符(T52 插件沙箱)",
    "E6010": "comptime 预算超限",
    "E6020": "comptime 副作用/不确定",
    "E6030": "comptime 反射泛型运行时类型(parametricity)",
    "E6040": "单态化实例预算超限(§8.5;CTRON_MONO_BUDGET 可调,默认 8192)",
    "W8010": "struct 含可变类引用字段(浅共享)",
    "W8020": "must-use 结果被丢弃",
    "W8030": "未使用绑定",
    "W8040": "遮蔽前奏符号",
    "W8050": "extern 未标记 #[trusted](信任边界;§9.6)",
    "W8051": "repr(c) struct 含非 C-ABI 字段(§9.6 v0.6)",
    "W8052": "extern 形参/返回非 C-ABI 类型(§9.6 v0.6)",
    "W8053": "extern 返回 fn 类型(dormant:v0.7 返回向合法化,码位保留)",
    "W9001": "fn 顶层语句数超限(T52 官方 lint 样例码;第三方 W9xxx 须经清单 plugin codes 声明)",
}

MARKER_RE = re.compile(r"^//@\s*(\w+)\s*:\s*(.+?)\s*$")
CODE_RE = re.compile(r"^[EW]\d{4}$")
TARGETS = {"full", "web", "bare"}


def kind_of(path: Path) -> str | None:
    name = path.name
    if name.endswith(".neg.ct"):
        return "neg"
    if name.endswith(".lint.ct"):
        return "lint"
    if name.endswith(".panic.ct"):
        return "panic"
    if name.endswith(".ct"):
        return "behavior"
    return None


def check_file(path: Path) -> list[str]:
    errors = []
    relparts0 = path.relative_to(ROOT).parts
    if "gui" in relparts0:
        # gui/ 泳道夹具由 run.sh 驱动(CTML S 泳道:构建+链接为主,窗口运行为交互验收);
        # 不按主流 test 块规则元检查(同 bench 夹具先例)。计划:2026-09-16-gui-mvp-ladder。
        return errors
    if relparts0 and relparts0[0] == "plugins":
        # plugins/ 泳道:T52 插件沙箱夹具(run.sh 自驱;包形多文件 + lint 夹具为
        # main 程序形非 test 块语义;gui/dist 泳道先例)。W9xxx 第三方码纪律 =
        # 插件清单 plugin 块 codes 先进表(§10.6),由阶梯与清单校验承担。
        return errors
    if "dist" in relparts0:
        # dist/ 分发夹具为普通 main 程序,由 ctc.sh/ctron-cc 直驱 + expected/*.out 黄金对照
        # (工具链分发线);不按主流 test 块规则元检查(同 gui/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] in ("doc_fix", "doc_fix_neg"):
        # doc_fix|doc_fix_neg:ctron doc 命令黄金夹具(闭源包分发线 S0.6/S0.7a),
        # 由 doc 套件直驱;不按主流 test 块规则元检查(同 gui/dist 泳道先例)。
        return errors
    if relparts0 and relparts0[0] == "doctest":
        # doctest/ 泳道:doc-test(§10.4)机制语料,tests/doctest/run.sh 直驱
        # bin/ctron-cc/ctron-emit 双面 rc 期望(含故意失败件),非 test 块语义
        # (gui/log 泳道先例);格式锚 00_doctest.ct 在根,走主流元检查。
        return errors
    if len(relparts0) >= 3 and relparts0[0] == "lang" and relparts0[1] == "bench":
        # lang/bench:T32/T44 性能核(bench.sh 自驱,无 test 块语义;http/bench 先例)。
        return errors
    if len(relparts0) >= 3 and relparts0[0] == "http" and relparts0[1] in ("bench", "fuzz"):
        # http/bench|fuzz:性能/模糊驱动(bench.sh、fuzz/run.sh 自驱,非 test 块语义;
        # 同 gui/ 泳道"bench 夹具先例")。
        return errors
    if relparts0 and relparts0[0] == "w7" and "src" in relparts0:
        # w7 泳道(T34 栈经济):tests/w7/run.sh 驱动,非 test 块语义(gc/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] == "gc" and "src" in relparts0:
        # gc 包 main:由 tests/gc/run.sh 双档(gc=on/gc=off)驱动,非 test 块语义
        # (net/ 泳道先例;T29)。
        return errors
    if relparts0 and relparts0[0] == "bare":
        # bare 泳道(T40-T42):裸机语料由 tests/bare/run.sh 双靶 qemu 驱动,
        # hello.ct 为 bare-metal main 程序无 test 块语义(net/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] == "wasm":
        # wasm 泳道(T37):wasm 语料由 tests/wasm/run.sh 交叉编译+node 真跑驱动,
        # hello/arith/strfmt 为 main 程序、ffi.neg 为构建面负锚(工具链无关文案),
        # 非 test 块语义(bare/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] == "net" and "src" in relparts0:
        # net 包 main:由 tests/net/run.sh 双臂(原生==解释黄金对照)驱动,
        # 不按主流 test 块规则元检查(同 dist/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] == "log" and "corpus" in relparts0:
        # log/corpus:std/log 语料(级别族/转义/门控矩阵),由 tests/log/run.sh 驱动,
        # 与 std/log inline 测试互独立;非 test 块语义(同 gui/ 泳道先例)。
        return errors
    if relparts0 and relparts0[0] in ("connect", "metrics", "ndjson", "otlp", "pb", "trace", "s3", "pkg", "rt_scopes"):
        # 服务器泳道 P7 语料/夹具目录:各自 run.sh 驱动、非 test 块语义
        # (gui/ 泳道先例;gotcha #17:新泳道 = meta_check 加泳道 skip)。
        return errors
    for pkg_lane in ("artifact_demo", "realdep_demo"):
        if relparts0 and relparts0[0] == pkg_lane:
            # 包分发泳道夹具(编译/分发线):库源文件被 consumer 包 import,
            # 不按主流 test 块规则元检查(同 dist/ 泳道先例)。
            return errors
    if relparts0 and relparts0[0] == "http" and len(relparts0) > 2:
        # http/ 泳道子目录源文件(frm_route 等由 http/run.sh 驱动,非 test 块语义)
        return errors
    kind = kind_of(path)
    if kind is None:
        return errors

    markers: dict[str, list[str]] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = MARKER_RE.match(line.strip())
        if not m:
            continue
        key, value = m.group(1), m.group(2)
        markers.setdefault(key, []).append(value)

    has_test_block = 'test "' in path.read_text(encoding="utf-8")

    # 多文件用例(tests/modules/<case>/ 与 tests/ffi/<case>/src/):类型由标记决定,
    # 源码文件跳过(bench/ 夹具为普通 main 程序,由 bench_ffi.sh 驱动)
    relparts = path.relative_to(ROOT).parts
    in_modules = "modules" in relparts or ("ffi" in relparts and "src" in relparts)
    if in_modules:
        # T49 装载器负例夹具:清单本身即 E5049 负样本(三互斥同现),由 smoke
        # 装载器断言驱动——不走清单 judge(同 gui/net 泳道 skip 先例)
        case_dirs_negative_manifest = {"dep_mutex_neg"}
        if len(relparts) >= 2 and relparts[1] in case_dirs_negative_manifest:
            return errors
        if not (path.parent.parent / "Ctron.ctcl").exists():
            errors.append("modules 用例缺少 Ctron.ctcl(项目根)")
        else:
            mtext = (path.parent.parent / "Ctron.ctcl").read_text(encoding="utf-8")
            ok, mds = ctcl_check.judge_v1(mtext)
            if not ok:
                for d in mds:
                    if d["code"].startswith("E"):
                        errors.append(f"Ctron.ctcl {d['code']} L{d['line']}: {d['msg']}")
        if markers.get("fail"):
            kind = "neg"
        elif markers.get("warn"):
            kind = "lint"
        elif markers.get("panic"):
            kind = "panic"
        elif has_test_block:
            kind = "behavior"
        else:
            return errors          # 普通源码文件(如 circular/src/b.ct)

    # 未知标记键
    for key in markers:
        if key not in {"fail", "msg", "warn", "panic", "target"}:
            errors.append(f"未知标记键: //@ {key}:")
    # msg 只配 fail
    if "msg" in markers and "fail" not in markers:
        errors.append("//@ msg: 只能与 //@ fail: 同用")
    # 错误码必须已注册(点式变体码合法:E2020.use.nat 等 P1b 整键前缀;基码须注册)
    for key in ("fail", "warn"):
        for code in markers.get(key, []):
            base = code.split(".")[0]
            if not CODE_RE.match(base) or (code != base and not re.fullmatch(r"(\.[a-z][a-z_]*)+", code[len(base):])):
                errors.append(f"错误码格式非法: {code}")
            elif base not in ERROR_CODES:
                errors.append(f"错误码未注册(先加 README §4 与本脚本注册表): {code}")
    # target/profile 取值
    for key in ("target", "profile"):
        for value in markers.get(key, []):
            if value not in TARGETS:
                errors.append(f"{key} 取值非法: {value}(应为 {sorted(TARGETS)})")

    if kind == "neg":
        if not markers.get("fail"):
            errors.append("neg 测试必须列出至少一个 //@ fail: 错误码")
        for banned in ("warn", "panic"):
            if banned in markers:
                errors.append(f"neg 测试不得使用 //@ {banned}:")
    elif kind == "lint":
        if not markers.get("warn"):
            errors.append("lint 测试必须列出至少一个 //@ warn: 警告码")
        if "fail" in markers or "panic" in markers:
            errors.append("lint 测试不得 fail/panic(应编译通过)")
        if not has_test_block:
            errors.append("lint 测试须含至少一个 test 块(应可编译可运行)")
    elif kind == "panic":
        if not markers.get("panic"):
            errors.append("panic 测试必须有 //@ panic: 标记")
        if "fail" in markers or "warn" in markers:
            errors.append("panic 测试不得 fail/warn(应编译通过)")
        if not has_test_block:
            errors.append("panic 测试须含至少一个 test 块")
    else:  # behavior
        for banned in ("fail", "warn", "panic"):
            if banned in markers:
                errors.append(
                    f"行为测试不得使用 //@ {banned}:(请拆分为对应后缀的文件)"
                )
        if not has_test_block:
            errors.append("行为测试须含至少一个 test 块")

    return errors


CODE_LIT_RE = re.compile(r'"([EW]\d{4})(?:\.[A-Za-z0-9_.]+)?"')


def check_diag_catalog() -> list[str]:
    """§10.8 R1:诊断目录覆盖守卫——发射面用到的每个码,diag_msg.ct zh 表必须有键。"""
    errs: list[str] = []
    dm = ROOT.parent / "compiler" / "src" / "diag_msg.ct"
    if not dm.exists():
        return ["缺 diag_msg.ct(诊断单一出口;设计见 specs/2026-09-17-diag-i18n-error-code-audit.md)"]
    catalog = set(CODE_LIT_RE.findall(dm.read_text(encoding="utf-8")))
    emitted: set[str] = set()
    for p in sorted((ROOT.parent / "compiler" / "src").glob("*.ct")):
        if p.name == "diag_msg.ct":
            continue
        for ln in p.read_text(encoding="utf-8").splitlines():
            s = ln.strip()
            if s.startswith("//"):
                continue  # 注释里的码(如 dormant W8053)不要求目录覆盖
            emitted |= set(CODE_LIT_RE.findall(ln))
    missing = sorted(emitted - catalog)
    if missing:
        errs.append("诊断码未入目录(先加 diag_msg.ct diag_tpl_zh 再使用): " + ",".join(missing))
    return errs


CACHE_CLEAN_RE = re.compile(r"((?<!-)\brm\b[^#\n]*|unlink\s+)[^\n]*\.cache")


def check_script_cache_cleanup() -> list[str]:
    """驱动器 clean 是缓存唯一合法清理口(设计 build-driver-design D5/§6):
    扫全部编排脚本,凡脚本私 rm/unlink .cache 路径 = 报错。只建(mkdir)不算。
    (?<!-) 排除 docker run --rm 旗标(tests/bare 沙盒读 .cache/bare/gate_$$ 内核镜像
    属读取非清理);行内 # 后剥除再匹配 = 注释豁免,同 check_diag_catalog 的
    strip 注释先例。"""
    errs: list[str] = []
    repo = ROOT.parent
    roots = [repo / d for d in ("examples", "tests", "tools", "selfhosted", "compiler")]
    scripts = sorted(
        p for root in roots if root.is_dir()
        for p in root.rglob("*.sh") if p.is_file()
    )
    ci = repo / "ci.sh"
    if ci.is_file():
        scripts.append(ci)
    for p in scripts:
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            code = line.split("#", 1)[0]  # 注释豁免:# 后剥除再匹配
            if CACHE_CLEAN_RE.search(code):
                errs.append(f"{p.relative_to(repo)}:{i}: 脚本私清缓存(唯一合法口 = ctron clean): {line.strip()}")
    return errs


def main() -> int:
    all_files = sorted(p for p in ROOT.rglob("*.ct") if p.is_file())
    stats: dict[str, int] = {}
    failures = 0

    for path in all_files:
        rel = path.relative_to(ROOT)
        # roadmap/ 锚点语料不按主流规则元检查(README §9:主流套件与 campaign 跳过
        # roadmap/ 前缀;锚点状态由 roadmap_suite 锚点表承载)
        if rel.parts and rel.parts[0] == "roadmap":
            continue
        kind = kind_of(path) or "other"
        stats[kind] = stats.get(kind, 0) + 1
        for err in check_file(path):
            failures += 1
            print(f"[FAIL] {rel}: {err}")

    for err in check_diag_catalog():
        failures += 1
        print(f"[FAIL] diag_msg.ct: {err}")

    for err in check_script_cache_cleanup():
        failures += 1
        print(f"[FAIL] {err}")

    print()
    print(f"测试文件: {len(all_files)} 个 " + " ".join(f"{k}={v}" for k, v in sorted(stats.items())))
    if failures:
        print(f"元检查失败: {failures} 处问题")
        return 1
    print("元检查通过:标记、命名约定与错误码引用全部一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
