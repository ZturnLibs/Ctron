#!/usr/bin/env python3
"""CTCL(Ctron Config Language)校验器 —— Python 参考实现(规范 config-language-v1 §9)。

机制:字符串感知注释、块/字段/四类值、注释所有权(字段级)、schema-as-CTCL 注册表
(注册表数据 = ctcl_manifest_schema.ctcl,加载器见 _load_schema)、E504x 诊断、
解析恢复策略(括号平衡跳读)、规范渲染与 fmt 幂等。
"""
import os, re

# 注册表三件套由 schema 文件驱动(_load_schema,文件尾初始化);形态与旧硬编码一致。
SCHEMA = {}
KNOWN_CAPS = set()
BLOCK_ORDER = {}
IDENT = r"[a-z_][a-z0-9_]*"
HEAD = re.compile(r"(%s)(\s*(\"(?:[^\"\\]|\\.)*\"))?\s*\{\s*$" % IDENT)
FIELD = re.compile(r"(%s)\s*=\s*(.+?)\s*$" % IDENT)

# schema 方言允许的旗标(reg 块级 / regkey 键级);未知旗标 = 加载失败(L0 纪律)
_REG_FLAGS = {"kind", "required", "name_pattern", "mutex"}
_REGKEY_FLAGS = {"type", "const", "first", "min", "pattern", "members", "sorted", "required"}
_TYPES = {"str", "int", "bool", "list"}


def _load_schema(path):
    """schema-as-CTCL:解析 schema 文件(文法复用 parse_manifest)→ 注册表三件套。"""
    with open(path, encoding="utf-8") as fh:
        blocks, ds, _ = parse_manifest(fh.read())
    es = [d for d in ds if d["code"].startswith("E")]
    if es:
        raise RuntimeError("schema 文法错误 %s:%s" % (path, es[0]))
    schema, block_order, known_lists = {}, {}, {}
    saw_version = False
    for b in blocks:
        if b["name"] == "schema":
            for f in b["fields"]:
                if f["k"] == "schema_version" and f["v"] == 1:
                    saw_version = True
        elif b["name"] == "reg":
            name = b["arg"]
            if name is None or name in schema:
                raise RuntimeError("schema:reg 块缺名字实参或重复:%r" % name)
            spec = {"keys": {}}
            for f in b["fields"]:
                if f["k"] not in _REG_FLAGS:
                    raise RuntimeError("schema:reg %s 未知旗标 %s" % (name, f["k"]))
                spec[f["k"]] = f["v"]
            if spec.get("kind") not in ("record", "keyed"):
                raise RuntimeError("schema:reg %s kind 非法" % name)
            if "mutex" in spec:
                spec["mutex"] = [tuple(g.split("+")) for g in spec["mutex"]]
            schema[name] = spec
            block_order[name] = len(block_order)
        elif b["name"] == "regkey":
            arg = b["arg"] or ""
            blk, _, key = arg.partition(".")
            if not blk or not key or blk not in schema:
                raise RuntimeError("schema:regkey 路径非法 %r" % arg)
            spec = {}
            for f in b["fields"]:
                if f["k"] not in _REGKEY_FLAGS:
                    raise RuntimeError("schema:regkey %s 未知旗标 %s" % (arg, f["k"]))
                if f["k"] == "type" and f["v"] not in _TYPES:
                    raise RuntimeError("schema:regkey %s type 非法" % arg)
                spec["req" if f["k"] == "required" else f["k"]] = f["v"]
            schema[blk]["keys"][key] = spec
            if "members" in spec:
                known_lists[(blk, key)] = list(spec["members"])
        else:
            raise RuntimeError("schema:未知顶层块 %s(合法:schema/reg/regkey)" % b["name"])
    if not saw_version:
        raise RuntimeError("schema:缺 schema_version = 1")
    known = set()
    for (blk, key), members in known_lists.items():
        known.update(members)
    return schema, known, block_order


def diag(code, ln, msg): return {"code": code, "line": ln, "msg": msg}


def strip_comment(s, ln, ds):
    """字符串感知剥离注释。# 触发 E5042 专用提示后按 // 恢复(不雪崩)。"""
    in_str = esc = False
    for i, c in enumerate(s):
        if in_str:
            if esc: esc = False
            elif c == "\\": esc = True
            elif c == '"': in_str = False
        else:
            if c == '"': in_str = True
            elif c == "/" and s[i:i+2] == "//":
                return s[:i], s[i+2:].strip()
            elif c == "#":
                ds.append(diag("E5042", ln, "本语言注释是 // 而非 #(与宿主语言一致);本行已按 // 恢复"))
                return s[:i], s[i+1:].strip()
    if in_str:
        ds.append(diag("E5040", ln, "字符串未闭合")); return None, None
    return s, None

def parse_string(s, ln, ds):
    if len(s) < 2 or s[0] != '"' or s[-1] != '"': return None
    body, out, esc = s[1:-1], [], False
    for c in body:
        if esc:
            if c in ('"', "\\"): out.append(c)
            else:
                ds.append(diag("E5048", ln, "非法字符串值(转义只允许 反斜杠加引号 与 双反斜杠)")); return None
            esc = False
        elif c == "\\": esc = True
        elif ord(c) < 0x20:
            ds.append(diag("E5048", ln, "非法字符串值(含裸控制字符)")); return None
        else: out.append(c)
    if esc:
        ds.append(diag("E5048", ln, "非法字符串值(孤立的尾部反斜杠)")); return None
    return "".join(out)

def parse_value(raw, ln, ds):
    raw = raw.strip()
    if raw.startswith("["):
        if not raw.endswith("]"):
            ds.append(diag("E5040", ln, "列表必须单行且以 ] 结尾;若元素是复杂结构,请改用键控块(块 \"名\" { ... })而非多行列表")); return None
        body = raw[1:-1].strip()
        if body == "": return []
        parts = [p.strip() for p in body.split(",")]
        if parts[-1] == "":
            ds.append(diag("E5048", ln, "列表不允许尾逗号(最后元素后直接 ']')")); return None
        out = []
        for p in parts:
            v = parse_string(p, ln, ds)
            if v is None:
                if not p.startswith('"'):
                    ds.append(diag("E5048", ln, "列表元素必须是双引号字符串;复杂结构请用键控块")); return None
                return None
            out.append(v)
        return out
    if raw in ("true", "false"): return raw == "true"
    if raw.startswith("'"):
        ds.append(diag("E5048", ln, "不支持单引号字符串(唯一拼写:双引号)")); return None
    if raw.startswith('"'): return parse_string(raw, ln, ds)
    if re.match(r"-?(0|[1-9][0-9]*)\Z", raw):
        n = int(raw)
        if -(2**63) <= n < 2**63: return n
        ds.append(diag("E5048", ln, "整数超出 I64")); return None
    if re.match(r"-?[0-9]*\.[0-9]", raw):
        ds.append(diag("E5040", ln, "不支持浮点;数值配置一律定点整数(如 85 表 85%)")); return None
    if raw.startswith("{"):
        ds.append(diag("E5040", ln, "不支持内联表;记录一律用块表达(dep \"名\" { ... })")); return None
    ds.append(diag("E5040", ln, "无法识别的值:%s" % raw)); return None

def parse_manifest(text):
    ds, blocks, lead, cur, closed = [], [], [], None, False
    skipping, bal = False, 0
    for ln, orig in enumerate(text.split("\n"), 1):
        code, trail = strip_comment(orig, ln, ds)
        if code is None: continue
        line = code.strip()
        if line == "":   # 空行,或整行注释(注释文本在 trail;修复:不得丢弃)
            (cur["free"] if cur else lead).append(trail if trail else None)
            continue
        if cur is not None and skipping:      # F6:跳读模式,按括号平衡吞到错误构造闭合为止
            bal += line.count("{") - line.count("}")
            if bal <= 0:
                skipping = False
            continue
        if cur is None:
            m = HEAD.match(line)
            if m:
                arg = None
                if m.group(3) is not None:
                    if line[len(m.group(1)):][:1] not in (" ", "\t"):
                        # 恢复策略:提示后仍开块,避免雪崩
                        ds.append(diag("E5040", ln, "块名与名字实参之间需要空格:dep \"名\""))
                    arg = parse_string(m.group(3), ln, ds)
                cur = {"name": m.group(1), "arg": arg, "ln": ln, "head_trail": trail,
                       "lead": lead[:], "free": [], "fields": []}
                lead = []
                continue
            if line.startswith("["):
                ds.append(diag("E5040", ln, "本语言不用 [section] 段头;请用块:pkg { ... } / dep \"名\" { ... }"))
            else:
                ds.append(diag("E5040", ln, "块外只允许块头(NAME [\"名\"]) {"))
            continue
        if line == "}":
            blocks.append(cur); cur = None; closed = True; continue
        m = FIELD.match(line)
        if not m:
            if line.startswith("{"):
                # JSON/YAML 肌肉记忆高频错法:值位置写对象字面量 → 专码单报 + 跳读
                ds.append(diag("E5040", ln, "不支持内联表;复杂记录请用键控块表达(块 \"名\" { ... })"))
                bal = line.count("{") - line.count("}")
                skipping = True
            elif "{" in line or "}" in line:
                # F6:嵌套/单行块 → 专码 + 跳读恢复(按括号平衡吞到构造闭合),一个错误只报一次
                ds.append(diag("E5040", ln, "块内禁止嵌套块/单行块(深度恒 1);此块已被跳过"))
                bal = line.count("{") - line.count("}")
                skipping = True
            else:
                ds.append(diag("E5040", ln, "块内每行必须是 键 = 值"))
                cur["fields"].append({"k": "\x00坏行", "v": None, "ln": ln,
                                      "trail": None, "lead": []})
            continue
        v = parse_value(m.group(2), ln, ds)
        f = {"k": m.group(1), "v": v, "ln": ln, "trail": trail, "lead": []}
        while cur["free"] and cur["free"][-1] is not None:   # 所有权:独立注释归其后第一个字段
            f["lead"].insert(0, cur["free"].pop())
        while cur["free"] and cur["free"][-1] is None: cur["free"].pop()
        cur["fields"].append(f)
    if cur is not None:
        ds.append(diag("E5040", cur["ln"], "块未闭合(缺 })"))
    # 注:"缺 pkg 块"由 schema 校验统一负责,解析层不重复报(F3:去重)
    tail = [c for c in lead if c is not None]   # 文件尾独立注释归文件
    return blocks, ds, tail

def lev(a, b):
    la, lb = len(a), len(b)
    dp = list(range(lb + 1))
    for i in range(1, la + 1):
        prev = dp[0]
        dp[0] = i
        for j in range(1, lb + 1):
            cur = dp[j]
            dp[j] = min(dp[j] + 1, dp[j - 1] + 1, prev + (a[i - 1] != b[j - 1]))
            prev = cur
    return dp[lb]


def did_you_mean(k, known):
    # 跨语言规则:Levenshtein 距离 ≤2 取最小者(平局取注册表序)
    best, bestd = None, 99
    for cand in known:
        d = lev(k, cand)
        if d < bestd:
            bestd, best = d, cand
    if best is not None and bestd <= 2:
        return ";你是不是想要 %s?" % best
    return ""

def validate(blocks, ds):
    seen = {}
    for b in blocks:
        spec = SCHEMA.get(b["name"])
        if spec is None:
            ds.append(diag("E5044", b["ln"], "未知块 %s;合法块:%s" %
                           (b["name"], ", ".join(sorted(SCHEMA))))); continue
        if spec["kind"] == "record" and b["arg"] is not None:
            ds.append(diag("E5041", b["ln"], "%s 是记录块,不带名字实参" % b["name"])); continue
        if spec["kind"] == "keyed":
            if b["arg"] is None:
                ds.append(diag("E5041", b["ln"], "%s 是键控块:dep \"名\" { ... }" % b["name"])); continue
            if not re.match(spec["name_pattern"] + r"\Z", b["arg"]):
                ds.append(diag("E5048", b["ln"], "键控块名 %r 不符合包名规则" % b["arg"])); continue
            key = (b["name"], b["arg"])
            if key in seen:
                ds.append(diag("E5045", b["ln"], "重复的 %s \"%s\"(首次在第 %d 行);同名块禁止追加"
                               % (b["name"], b["arg"], seen[key])))
            seen[key] = b["ln"]
        keys = [f["k"] for f in b["fields"]]
        for f in b["fields"]:
            if f["k"] == "\x00坏行": continue
            if keys.count(f["k"]) > 1:
                first = keys.index(f["k"])
                if b["fields"].index(f) != first: continue   # 重复键只在首现处报一次
                ds.append(diag("E5045", f["ln"], "重复键 %s(同名键只允许一次)" % f["k"])); continue
            ks = spec["keys"].get(f["k"])
            if ks is None:
                ds.append(diag("E5043", f["ln"], "块 %s 中未知键 %s%s;合法键:%s" %
                               (b["name"], f["k"], did_you_mean(f["k"], spec["keys"]),
                                ", ".join(spec["keys"])))); continue
            v, t = f["v"], ks["type"]
            if v is None: continue                            # 值已报过错,键视为存在,不雪崩
            ok = ((t == "str" and isinstance(v, str))
                  or (t == "int" and isinstance(v, int) and not isinstance(v, bool))
                  or (t == "list" and isinstance(v, list)))
            if not ok:
                ds.append(diag("E5046", f["ln"], "%s 的类型应为 %s" % (f["k"], t))); continue
            if t == "list":
                members = ks.get("members")
                for x in v:
                    if not isinstance(x, str):
                        ds.append(diag("E5046", f["ln"], "列表元素必须是字符串")); break
                    if members and x not in members:
                        ds.append(diag("E5043", f["ln"], "未知能力 %s;合法:%s(能力是安全边界,未知即拒绝)"
                                       % (x, ", ".join(sorted(members)))))
            if "const" in ks and v != ks["const"]:
                ds.append(diag("E5050", f["ln"], "manifest_version 必须为 %s" % ks["const"]))
            if "pattern" in ks and isinstance(v, str) and not re.match(ks["pattern"] + r"\Z", v):
                ds.append(diag("E5046", f["ln"], "键 %s 值 %r 不符合 %s" % (f["k"], v, ks["pattern"])))
            if "min" in ks and isinstance(v, int) and v < ks["min"]:
                ds.append(diag("E5046", f["ln"], "键 %s 必须 >= %s" % (f["k"], ks["min"])))
        if spec["kind"] == "record":
            for k, ks in spec["keys"].items():
                if ks.get("first"):
                    if k not in keys:
                        # F4:版本键"缺席"是语义错误(E);仅"次序不对"才是呈现问题(W)
                        ds.append(diag("E5050", b["ln"], "%s 缺语言版本键 %s(必须存在且 = 1)" % (b["name"], k)))
                    elif keys[0] != k:
                        ds.append(diag("W5051", b["ln"], "%s 块第一个键建议为 %s(规范形态;fmt 可自动归位)" % (b["name"], k)))
                if ks.get("req") and k not in keys:
                    ds.append(diag("E5047", b["ln"], "%s 缺必填键 %s" % (b["name"], k)))
        if spec["kind"] == "keyed":
            present = {f["k"] for f in b["fields"]}
            hit = None
            for combo in spec["mutex"]:
                if set(combo) <= present:
                    if hit is not None:
                        ds.append(diag("E5049", b["ln"], "dep 来源互斥:%s 与 %s 同现"
                                       % ("/".join(hit), "/".join(combo))))
                    hit = combo
            if hit is None:
                ds.append(diag("E5049", b["ln"], "dep 需要且仅需要一种来源:path | git+rev | version"))
    for bname, bspec in SCHEMA.items():
        if bspec["kind"] != "record":
            continue
        insts = [b for b in blocks if b["name"] == bname]
        if bspec.get("required") and not insts:
            ds.append(diag("E5047", 1, "缺 %s 块" % bname))
        elif len(insts) > 1:
            ds.append(diag("E5045", insts[1]["ln"], "%s 块最多一个" % bname))
    return ds

def canon_str(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def canon_value(v):
    if isinstance(v, bool): return "true" if v else "false"
    if isinstance(v, int): return str(v)
    if isinstance(v, list): return "[" + ", ".join(canon_str(x) for x in v) + "]"
    return canon_str(v)

def render(blocks, tail):
    def sk(b): return (BLOCK_ORDER.get(b["name"], 9), b["arg"] or "")
    chunks = []
    for b in sorted(blocks, key=sk):
        lines = []
        for c in (x for x in b["lead"] if x): lines.append("// " + c)
        head = b["name"] + (" " + canon_str(b["arg"]) if b["arg"] else "") + " {"
        if b["head_trail"]: head += "  // " + b["head_trail"]
        lines.append(head)
        pos = {k: i for i, k in enumerate(SCHEMA.get(b["name"], {"keys": {}})["keys"])}
        for f in sorted(b["fields"], key=lambda f: pos.get(f["k"], 99)):
            if f["k"] == "\x00坏行" or f["v"] is None: continue   # 损坏字段不进规范形态(F2)
            fspec = SCHEMA.get(b["name"], {"keys": {}})["keys"].get(f["k"], {})
            v = sorted(f["v"]) if (isinstance(f["v"], list) and fspec.get("sorted")) else f["v"]  # 集合语义:规范形态排序(schema sorted 旗标)
            for c in f["lead"]:
                if c: lines.append("    // " + c)
            row = "    " + f["k"] + " = " + canon_value(v)
            if f["trail"]: row += "  // " + f["trail"]
            lines.append(row)
        for c in (x for x in b["free"] if x): lines.append("    // " + c)
        lines.append("}")
        chunks.append("\n".join(lines))
    out = "\n\n".join(chunks)                       # §8.3 块间恰一空行
    if tail:
        out += "\n\n" + "\n".join("// " + c for c in tail)
    return out + "\n"

FAILURES = []

def check(name, text, want):
    blocks, ds, tail = parse_manifest(text)
    ds = validate(blocks, ds)
    c1 = render(blocks, tail)
    b2, d2, t2 = parse_manifest(c1)
    idem = (not d2) and render(b2, t2) == c1
    es = [d for d in ds if d["code"].startswith("E")]
    ws = [d for d in ds if d["code"].startswith("W")]
    ok = len(es) == want and idem
    print("[%s] %-30s E=%d(期望%d) W=%d fmt幂等=%s" %
          ("PASS" if ok else "FAIL", name, len(es), want, len(ws), "Y" if idem else "N"))
    for d in ds: print("      %s L%d: %s" % (d["code"], d["line"], d["msg"]))
    if not idem:
        print("".join(difflib.unified_diff(c1.splitlines(1), render(b2, t2).splitlines(1),
                                           "canon1", "canon2")))
    if not ok: FAILURES.append(name)

POS = [
("ctwf 纯 pkg", '''\
// ctwf 包清单(Ctron 包清单格式 v1)
pkg {
    manifest_version = 1
    name    = "ctwf"
    version = "0.1.0"
    caps    = ["time"]
}
'''),
("path_dep app", '''\
pkg {
    manifest_version = 1
    name = "pathapp"
    version = "0.1.0"
}

dep "libmath" {
    path = "../lib"
}
'''),
("caps 声明", '''\
pkg {
    manifest_version = 1
    name = "capsfs"
    version = "0.1.0"
    caps = ["time"]        // 只声明 time,未声明 fs
}
'''),
("comptime 预算", '''\
pkg {
    manifest_version = 1
    name = "app"
    version = "0.1.0"
}

comptime {
    budget_ms = 10
}
'''),
("git 依赖(URL 含 //)+ 块前注释", '''\
pkg {
    manifest_version = 1
    name = "webapp"
    version = "0.1.0"
}

// 社区镜像,主仓在 github
dep "ctron-http" {
    git = "https://github.com/ctron/http"
    rev = "v1.2.0"
}
'''),
("注释所有权+乱序键(渲染须保真)", '''\
pkg {
    name = "z"
    // 格式版本,勿动
    manifest_version = 1
    version = "0.1.0"
}
'''),
("空列表 caps", '''\
pkg {
    manifest_version = 1
    name = "bare-app"
    version = "0.1.0"
    caps = []
}
'''),
("转义与路径形态", '''\
pkg {
    manifest_version = 1
    name = "edge"
    version = "0.1.0"
    caps = ["fs", "time"]
}

dep "wintool" {
    path = "C:\\\\tools\\\\lib"
}
'''),
]
NEG = [
("旧 [section] 方言(3 条独立诊断)", '[package]\nname = "x"\n', 3),
("# 注释恢复为 //", 'pkg {\n    manifest_version = 1\n    name = "x" # hi\n    version = "0.1.0"\n}\n', 1),
("列表尾逗号", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n    caps = ["time",]\n}\n', 1),
("重复 dep 同名", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n}\n\ndep "a" {\n    path = "../a"\n}\n\ndep "a" {\n    path = "../a2"\n}\n', 1),
("未知键 did-you-mean(2 条独立诊断)", 'pkg {\n    manifest_version = 1\n    nam = "x"\n    version = "0.1.0"\n}\n', 2),
("dep 来源互斥", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n}\n\ndep "a" {\n    path = "../a"\n    version = "1.0"\n}\n', 1),
("未知能力 fail-closed", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n    caps = ["net"]\n}\n', 1),
("浮点拒绝(带定点建议)", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n}\n\ncomptime {\n    budget_ms = 1.5\n}\n', 1),
("单引号拒绝(键视为存在,不雪崩)", "pkg {\n    manifest_version = 1\n    name = 'x'\n    version = \"0.1.0\"\n}\n", 1),
("缺 manifest_version", 'pkg {\n    name = "x"\n    version = "0.1.0"\n}\n', 1),
("单行块拒绝", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n}\n\ndep "a" { path = "../a" }\n', 1),
("块名与实参无空格(恢复后 1 条)", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n}\n\ndep"a" {\n    path = "../a"\n}\n', 1),
("嵌套块拒绝(跳读恢复,1 条)", 'pkg {\n    manifest_version = 1\n    name = "x"\n    version = "0.1.0"\n    inner {\n        a = 1\n    }\n}\n', 1),
]
def judge_v1(text):
    """判定器:E=0 即通过(允许 W)。返回 (ok, diags)。"""
    blocks, ds, tail = parse_manifest(text)
    ds = validate(blocks, ds)
    es = [d for d in ds if d["code"].startswith("E")]
    return (len(es) == 0), ds

# schema-as-CTCL 初始化(§9 L2):注册表 = ctcl_manifest_schema.ctcl 的数据
SCHEMA, KNOWN_CAPS, BLOCK_ORDER = _load_schema(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "ctcl_manifest_schema.ctcl"))

# ---------------- CLI ----------------
def selftest():
    for name, text in POS: check(name, text, 0)
    for name, text, want in NEG: check(name, text, want)
    print("\n总体:", "ALL GREEN" if not FAILURES else "HAS FAILURES: %s" % FAILURES)
    return not FAILURES

def cli(argv):
    import os, sys
    if "--selftest" in argv:
        return 0 if selftest() else 1
    fmt_mode = "--fmt" in argv
    paths = [a for a in argv if not a.startswith("-")]
    if not paths:
        print("用法:ctcl_check.py [--fmt] [--selftest] <文件|目录>…  (目录 → 递归找 Ctron.ctcl)")
        return 2
    files = []
    for p in paths:
        if os.path.isfile(p):
            files.append(p)
        elif os.path.isdir(p):
            for root, _dirs, names in os.walk(p):
                files.extend(os.path.join(root, n) for n in sorted(names) if n.endswith(".ctcl"))
        else:
            print("[SKIP] 不存在:%s" % p)
    bad = 0
    for f in files:
        with open(f, encoding="utf-8") as fh:
            text = fh.read()
        ok, ds = judge_v1(text)
        es = [d for d in ds if d["code"].startswith("E")]
        ws = [d for d in ds if d["code"].startswith("W")]
        print("[%s] %s  E=%d W=%d" % ("PASS" if ok else "FAIL", f, len(es), len(ws)))
        for d in ds:
            print("      %s L%d: %s" % (d["code"], d["line"], d["msg"]))
        if ok and fmt_mode:
            blocks, _, tail = parse_manifest(text)
            print(render(blocks, tail), end="")
        if not ok: bad += 1
    print("\nctcl_check:%d/%d 通过" % (len(files) - bad, len(files)))
    return 1 if bad else 0

if __name__ == "__main__":
    import sys
    sys.exit(cli(sys.argv[1:]))
