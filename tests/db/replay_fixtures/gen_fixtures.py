#!/usr/bin/env python3
# tests/db/replay_fixtures/gen_fixtures.py —— 回放夹具生成器(P5-C)
# 录制来源登记:本波夹具 = 手工按 PostgreSQL 线协议 v3 公开规范构造
# (PostgreSQL 文档 §53 Frontend/Backend Protocol),本生成器即构造过程
# 的可复核落盘(锚生成器纪律,P5-B 同款);nightly 真库导出钩子 Task 6
# 另标。产物:同目录 *.script(十六进制帧脚本,一帧一行,# 注释)。
# 用法:python3 gen_fixtures.py(幂等重写)。
import struct

def u16(v): return struct.pack(">H", v)
def u32(v): return struct.pack(">I", v)
def i32(v): return struct.pack(">i", v)
def cstr(s): return s.encode("utf-8") + b"\x00"

def frame(mtype: bytes, payload: bytes) -> str:
    """server 帧 = type(1) + len(4, 含自身) + payload"""
    body = mtype + u32(4 + len(payload)) + payload
    return body.hex()

def auth(kind: int, extra: bytes = b"") -> str:
    return frame(b"R", u32(kind) + extra)

def param_status(key: str, val: str) -> str:
    return frame(b"S", cstr(key) + cstr(val))

def backend_keydata(pid: int, key: int) -> str:
    return frame(b"K", u32(pid) + u32(key))

def ready(status: str) -> str:
    return frame(b"Z", status.encode())

def rowdesc(fields) -> str:
    """fields = [(name, type_oid)] —— 表 OID 0 / attnum 序 / typlen 按档 / typmod -1 / 文本格式"""
    p = u16(len(fields))
    for i, (name, oid) in enumerate(fields):
        p += cstr(name) + u32(0) + u16(i + 1) + u32(oid)
        typlen = {23: 4, 25: -1, 16: 1}[oid]
        p += struct.pack(">h", typlen) + i32(-1) + u16(0)
    return frame(b"T", p)

def datarow(cells) -> str:
    """cells = [bytes|None] —— None = NULL(列级 -1)"""
    p = u16(len(cells))
    for c in cells:
        if c is None:
            p += i32(-1)
        else:
            p += i32(len(c)) + c
    return frame(b"D", p)

def command_complete(tag: str) -> str:
    return frame(b"C", cstr(tag))

def error_response(fields: dict) -> str:
    p = b""
    for code, val in fields.items():
        p += code.encode() + cstr(val)
    p += b"\x00"
    return frame(b"E", p)

def write_script(name: str, header: list, lines: list):
    with open(name, "w") as f:
        f.write("# " + "\n# ".join(header) + "\n")
        for ln in lines:
            f.write(ln + "\n")
    print("wrote", name)

# ── startup_ok:握手 ok 路(AuthOk + 2×ParameterStatus + BackendKeyData + ReadyForQuery) ──
write_script("startup_ok.script", [
    "startup handshake ok-path (P5-C; gen_fixtures.py)",
    "seq: R(AuthOk=0) S(client_encoding) S(DateStyle) K(pid=4711,key=305419896) Z(idle)",
], [
    auth(0),
    param_status("client_encoding", "UTF8"),
    param_status("DateStyle", "ISO, MDY"),
    backend_keydata(4711, 305419896),
    ready("I"),
])

# ── query_rows:简单查询全行集(3 行 × int4/text/bool 混型,全非 NULL) ──
write_script("query_rows.script", [
    "simple query full rowset: 3 rows x (int4, text, bool) (P5-C)",
    "seq: T(id,name,active) D D D C(SELECT 3) Z(idle)",
], [
    rowdesc([("id", 23), ("name", 25), ("active", 16)]),
    datarow([b"1", b"alice", b"t"]),
    datarow([b"2", b"bob", b"f"]),
    datarow([b"3", b"carol", b"t"]),
    command_complete("SELECT 3"),
    ready("I"),
])

# ── error_response:错误响应面(SQLSTATE 42P01 传播) ──
write_script("error_response.script", [
    "error response surface: SQLSTATE propagation (P5-C)",
    "seq: E(S=V=FATAL C=42P01 M=relation.. P=15) Z(idle)",
    "query-side v0: E 即收口(err 5),尾 Z 留给会话层(TODO Task 4 会话游标)",
], [
    error_response({
        "S": "FATAL",
        "V": "FATAL",
        "C": "42P01",
        "M": 'relation "missing_table" does not exist',
        "P": "15",
    }),
    ready("I"),
])

# ── bad_length:协议错帧拒收面(坏长度/坏十六进制;clean Err 无崩溃无挂起) ──
write_script("bad_length.script", [
    "protocol-violation frames: clean Err, no crash/hang (P5-C)",
    "line 1: declared len 100 vs 1 payload byte (len/payload mismatch -> err 1)",
    "line 2: declared len 3 < 4 (length domain -> err 2)",
    "line 3: hi byte 0x01 = 16 MiB+ declared (domain gate -> err 2)",
    "line 4: non-hex digit (frame bad -> err 1)",
    "line 5: odd hex length (frame bad -> err 1)",
], [
    "52" + u32(100).hex() + "aa",
    (b"R" + u32(3)).hex(),
    (b"R" + b"\x01\x00\x00\x64" + b"\xaa").hex(),
    "zz",
    "5",
])

# ── empty_result:空结果集(T 一列 + C(SELECT 0) + Z;零 D) ──
write_script("empty_result.script", [
    "empty result set: RowDescription present, zero DataRows (P5-C)",
    "seq: T(count) C(SELECT 0) Z(idle)",
], [
    rowdesc([("count", 23)]),
    command_complete("SELECT 0"),
    ready("I"),
])

# ── null_value:NULL 值(列级 -1 长)+ 同行非 NULL 对照 ──
write_script("null_value.script", [
    "NULL value: column-level -1 length (P5-C)",
    "seq: T(memo,flag) D(NULL,'t') C(SELECT 1) Z(idle)",
], [
    rowdesc([("memo", 25), ("flag", 16)]),
    datarow([None, b"t"]),
    command_complete("SELECT 1"),
    ready("I"),
])

# ── utf8_bytes:多字节 UTF-8 文本格字节精确面(ASCII 解码面 fail-closed 锚) ──
write_script("utf8_bytes.script", [
    "multi-byte UTF-8 text cell: byte-exact face (P5-C)",
    "cell bytes = UTF-8 of U+4E2D (e4b8ad); ascii getter fails closed,",
    "hex/bytes getters carry exact payload (interp utf8_enc divergence)",
    "seq: T(txt) D('中') C(SELECT 1) Z(idle)",
], [
    rowdesc([("txt", 25)]),
    datarow(["中".encode("utf-8")]),
    command_complete("SELECT 1"),
    ready("I"),
])

# ── auth_stub:认证存根检测面(Cleartext=3 / SASL=10) ──
write_script("auth_stub.script", [
    "auth stub detection: cleartext(3) and SASL(10) kinds (P5-C; SCRAM = Task 4)",
    "line 1: R kind=3 (cleartext)",
    "line 2: R kind=10 + mechanism list 'SCRAM-SHA-256'",
    "handshake from cursor 0 -> err 4 authkind 3; from cursor 1 -> err 4 authkind 10",
], [
    auth(3),
    auth(10, cstr("SCRAM-SHA-256") + b"\x00"),
])

# ── 生成器自检:帧长域/字节面一致性 ──
assert len(frame(b"R", u32(0))) == 18  # 5 + 4 bytes hex
print("gen ok")
