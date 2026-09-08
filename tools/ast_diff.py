#!/usr/bin/env python3
"""ast_diff.py —— 自举差分闭环:Rust 版 AST vs C 版 AST(C-AST v1 契约)。

对 tests/ 语料逐文件:
  1. 运行 Rust `ctron parse <f> --ast`(多行 Rust Debug)与 C `ctronc parse <f> --ast`
     (单行 Rust-Debug 同族,ctron_file_show)。
  2. 两侧输出分别分词 → 解析为树(忽略空白与行布局;丢弃 closers 前的尾逗号)。
  3. 归一化后递归比较:
     - `Variant(Obj{...})` 与 `Variant{...}` 视为同形(C 侧把 decl 包裹层拍平);
     - 结构体字段按 名→值 字典比较(顺序不敏感),缺失/多余/不一致逐项报告;
     - 字符串反转义后比较(\\xNN 与 \\u{...} 等价);数字按数值比较。
  4. 诊断行只比较诊断码序列(E/W 编号),消息文本两侧契约不同、不比。

用法: python3 tools/ast_diff.py [--rust PATH] [--c PATH] [-v] [files...]
退出码: 全部一致 → 0,存在分歧 → 1。
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# ---------------- 分词 ----------------

TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<str>"(?:\\.|[^"\\])*")
  | (?P<num>-?\d+\.\d+(?:[eE][+-]?\d+)?|-?\d+)
  | (?P<ident>[A-Za-z_][A-Za-z0-9_]*)
  | (?P<punct>[{}\[\](),:])
""", re.VERBOSE)

def tokenize(text):
    toks, pos = [], 0
    while pos < len(text):
        m = TOKEN_RE.match(text, pos)
        if not m:
            raise ValueError(f"无法分词 @{pos}: {text[pos:pos+40]!r}")
        pos = m.end()
        if m.lastgroup == "ws":
            continue
        toks.append((m.lastgroup, m.group()))
    return toks

# ---------------- 树解析 ----------------

class P:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def take(self, expect=None):
        k, v = self.peek()
        if k is None:
            raise ValueError("意外结束")
        if expect and v != expect:
            raise ValueError(f"预期 {expect!r},实际 {v!r}")
        self.i += 1
        return k, v

    def value(self):
        k, v = self.peek()
        if k == "punct" and v == "(":
            # C 侧空指针防御输出 "(null)" → 视作 None
            self.take()
            _, name = self.take()
            self.take(")")
            return ("ident", name if name == "null" else f"({name})")
        if k == "str":
            self.take()
            return ("str", v)
        if k == "num":
            self.take()
            return ("num", v)
        if k == "ident":
            self.take()
            nk, nv = self.peek()
            if nv == "{":
                self.take()
                fields = {}
                while self.peek()[1] != "}":
                    fk = _, fname = self.take()
                    self.take(":")
                    fields[fname] = self.value()
                    if self.peek()[1] == ",":
                        self.take()
                self.take("}")
                return ("obj", v, fields)
            if nv == "(":
                self.take()
                args = []
                if self.peek()[1] != ")":
                    while True:
                        args.append(self.value())
                        if self.peek()[1] == ",":
                            self.take()
                            if self.peek()[1] == ")":
                                break
                            continue
                        break
                self.take(")")
                return ("call", v, args)
            return ("ident", v)
        if k == "punct" and v == "[":
            self.take()
            items = []
            while self.peek()[1] != "]":
                items.append(self.value())
                if self.peek()[1] == ",":
                    self.take()
            self.take("]")
            return ("list", items)
        raise ValueError(f"意外的记号 {v!r}")

def parse_tree(text):
    # 去掉 closers 前的尾逗号(Rust 多行 Debug 特有)
    text = re.sub(r",\s*([}\])])", r"\1", text)
    p = P(tokenize(text))
    tree = p.value()
    if p.i != len(p.toks):
        raise ValueError(f"尾部多余记号: {p.toks[p.i:p.i+5]}")
    return tree

def unwrap_some(node):
    """Option 拍平契约:C 侧有值直接打印、Rust 侧包 Some(...);统一解包 Some。
    仅作用于裸 Some 单参调用;用户枚举构造器在 AST 中位于 Call.callee 之下,
    不会以裸 Some 形态出现在字段位置。"""
    if node[0] == "call" and node[1] == "Some" and len(node[2]) == 1:
        return node[2][0]
    return node

# ---------------- 归一化比较 ----------------

def unescape(raw):
    body = raw[1:-1]
    out, i = [], 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        n = body[i + 1]
        mapping = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", '"': '"', "\\": "\\"}
        if n in mapping:
            out.append(mapping[n])
            i += 2
        elif n == "x":
            out.append(chr(int(body[i + 2:i + 4], 16)))
            i += 4
        elif n == "u":
            j = body.index("}", i)
            out.append(chr(int(body[i + 2:j], 16)))
            i = j + 1
        else:
            out.append(n)
            i += 2
    return "".join(out)

def try_flatten(a, b):
    """decl 包裹层调和:C 侧把 `Struct(StructDecl{...})` 拍平为 `Struct{...}`。
    仅当一侧是 call 且单参为 obj、另一侧是同名 obj 时才拍平(避免误伤
    `Tuple(Named{...})` 这类真实元组变体载荷)。"""
    if a[0] == "call" and b[0] == "obj" and a[1] == b[1] and len(a[2]) == 1 and a[2][0][0] == "obj":
        return ("obj", a[1], a[2][0][2]), b
    if b[0] == "call" and a[0] == "obj" and a[1] == b[1] and len(b[2]) == 1 and b[2][0][0] == "obj":
        return a, ("obj", b[1], b[2][0][2])
    return a, b

def splice_list_args(node):
    """Vec 载荷编码归一:Rust 的 Vec 只能编码为单列表参数 Array([...]),
    C 侧把元素直接作为参数打印 Array(x, y)。单列表参数 → 拆包为多项参数。"""
    if node[0] == "call" and len(node[2]) == 1 and node[2][0][0] == "list":
        return ("call", node[1], node[2][0][1])
    return node

def is_default(node):
    """C-AST v1 show 的既知省略字段常取这些'默认'值(vis: Private、空列表、None、false、0)。"""
    k = node[0]
    if k == "ident" and node[1] in ("Private", "None", "false"):
        return True
    if k == "list" and not node[1]:
        return True
    if k == "str" and unescape(node[1]) == "":
        return True
    if k == "num" and float(node[1]) == 0.0:
        return True
    return False

def cmp_tree(a, b, path, diffs, warns):
    a, b = unwrap_some(a), unwrap_some(b)
    a, b = try_flatten(a, b)
    ka, kb = a[0], b[0]
    if ka == "call" and kb == "call":
        a, b = splice_list_args(a), splice_list_args(b)
    if ka == "call" and kb == "call":
        a, b = splice_list_args(a), splice_list_args(b)
    if ka != kb:
        if ka in ("obj", "call") and kb in ("obj", "call"):
            diffs.append(f"{path}: 形态 {ka} vs {kb}({short(a)} vs {short(b)})")
        else:
            diffs.append(f"{path}: {short(a)} vs {short(b)}")
        return
    if ka == "str":
        ua, ub = unescape(a[1]), unescape(b[1])
        if ua != ub:
            diffs.append(f"{path}: 字符串 {a[1]} vs {b[1]}")
    elif ka == "num":
        if float(a[1]) != float(b[1]):
            diffs.append(f"{path}: 数值 {a[1]} vs {b[1]}")
    elif ka == "ident":
        if a[1] != b[1]:
            diffs.append(f"{path}: {a[1]} vs {b[1]}")
    elif ka == "list":
        if len(a[1]) != len(b[1]):
            diffs.append(f"{path}: 列表长度 {len(a[1])} vs {len(b[1])}({short(a)} vs {short(b)})")
        for i, (x, y) in enumerate(zip(a[1], b[1])):
            cmp_tree(x, y, f"{path}[{i}]", diffs, warns)
    elif ka == "obj":
        na, fb = a[1], b[1]
        if na != b[1]:
            diffs.append(f"{path}: 变体名 {na} vs {b[1]}")
            return
        fa, fb = a[2], b[2]
        for k in fa:
            if k not in fb:
                if is_default(fa[k]):
                    warns.append(f"{path}.{k}: C show 省略(默认值 {short(fa[k])})")
                else:
                    diffs.append(f"{path}.{k}: C 侧缺失({short(fa[k])})")
            else:
                cmp_tree(fa[k], fb[k], f"{path}.{k}", diffs, warns)
        for k in fb:
            if k not in fa:
                diffs.append(f"{path}.{k}: Rust 侧缺失({short(fb[k])})")
    elif ka == "call":
        if a[1] != b[1]:
            diffs.append(f"{path}: 调用名 {a[1]} vs {b[1]}")
            return
        if len(a[2]) != len(b[2]):
            diffs.append(f"{path}: 参数个数 {len(a[2])} vs {len(b[2])}")
            return
        for i, (x, y) in enumerate(zip(a[2], b[2])):
            cmp_tree(x, y, f"{path}({i})", diffs, warns)

def short(node, depth=0):
    if depth > 2:
        return "…"
    k = node[0]
    if k == "str":
        return node[1]
    if k in ("num", "ident"):
        return str(node[1])
    if k == "list":
        return "[" + ", ".join(short(x, depth + 1) for x in node[1][:3]) + "]"
    if k == "obj":
        return f"{node[1]}{{{', '.join(node[2])}}}"
    if k == "call":
        return f"{node[1]}(" + ", ".join(short(x, depth + 1) for x in node[2]) + ")"
    return "?"

# ---------------- 诊断码 ----------------

DIAG_RE = re.compile(r":\d+:\d+:?\s*((?:E|W)\d+)")

def diag_codes(output):
    codes = []
    for line in output.splitlines():
        m = DIAG_RE.search(line)
        if m and "decls," not in line:
            codes.append(m.group(1))
    return codes

# ---------------- 主流程 ----------------

def split_output(text):
    """诊断行与 AST 主体分离(AST 主体以首个 File { 开始)。"""
    lines = text.splitlines()
    diag, ast = [], []
    started = False
    for ln in lines:
        if not started and (ln.startswith("File {") or ln.startswith("File{")):
            started = True
        if started:
            ast.append(ln)
        elif DIAG_RE.search(ln):
            diag.append(ln)
    return "\n".join(ast), diag

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return r.stdout + r.stderr

def collect_files(explicit):
    if explicit:
        return [Path(f) for f in explicit]
    files = sorted((REPO / "tests").glob("*.ct"))
    files += sorted((REPO / "tests" / "modules").rglob("*.ct"))
    return files

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rust", default=str(REPO / "compiler-rust" / "target" / "debug" / "ctron"))
    ap.add_argument("--c", dest="cbin", default=str(REPO / "compiler-c" / "build" / "ctronc"))
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--max-show", type=int, default=6)
    ap.add_argument("files", nargs="*")
    args = ap.parse_args()

    ok = diff = 0
    warn_total = []
    for f in collect_files(args.files):
        r_out = run([args.rust, "parse", str(f), "--ast"])
        c_out = run([args.cbin, "parse", str(f), "--ast"])
        r_ast, r_diag = split_output(r_out)
        c_ast, c_diag = split_output(c_out)
        file_diffs = []
        rc, cc = diag_codes("\n".join(r_diag)), diag_codes("\n".join(c_diag))
        if rc != cc:
            file_diffs.append(f"诊断码序列: Rust={rc} vs C={cc}")
        file_warns = []
        try:
            rt = parse_tree(r_ast)
            ct = parse_tree(c_ast)
            cmp_tree(rt, ct, "File", file_diffs, file_warns)
        except ValueError as e:
            file_diffs.append(f"解析失败: {e}")
        if file_diffs:
            diff += 1
            print(f"DIFF {f}")
            for d in file_diffs[: args.max_show]:
                print(f"     {d}")
            if len(file_diffs) > args.max_show:
                print(f"     … 共 {len(file_diffs)} 处")
        else:
            ok += 1
            if file_warns:
                warn_total.extend((f, w) for w in file_warns)
                if args.verbose:
                    print(f"OK   {f}  (C show 省略 {len(file_warns)} 处)")
            elif args.verbose:
                print(f"OK   {f}")

    print(f"\n一致 {ok} / 分歧 {diff} / 共 {ok + diff}")
    if warn_total:
        seen = {}
        for f, w in warn_total:
            field = re.sub(r"\[\d+\]|\(\d+\)", "", w.split(":")[0]).split(".")[-1]
            seen.setdefault(field, [0, f])
            seen[field][0] += 1
        print("C show 省略(契约缺口,不计分歧):")
        for field, (n, f) in sorted(seen.items(), key=lambda kv: -kv[1][0]):
            print(f"  字段 `{field}`: {n} 处(示例 {f})")
    sys.exit(1 if diff else 0)

if __name__ == "__main__":
    main()
