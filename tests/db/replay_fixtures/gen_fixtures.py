#!/usr/bin/env python3
# tests/db/replay_fixtures/gen_fixtures.py —— 回放夹具生成器(P5-C/P5-D)
# 录制来源登记:本波夹具 = 手工按 PostgreSQL 线协议 v3 公开规范构造
# (PostgreSQL 文档 §53 Frontend/Backend Protocol),本生成器即构造过程
# 的可复核落盘(锚生成器纪律,P5-B 同款,幂等重写)。产物:
#   同目录 *.script               —— P5-C 帧(简单查询/握手/错帧)
#   ../replay_scram/*.script      —— P5-D 帧(SCRAM/扩展查询/事务/取消)
# SCRAM 夹具密码链 = RFC 5802 §5.1,向量 = RFC 7677 §3 示例(user/pencil),
# interp 臂夹具同链低迭代改制(i=1;emit 臂 x_scram_rfc7677 走原例 i=4096
# —— interp 堆不回收,P5-B 登记,c=4096 不可实用)。生成器自检:hashlib
# 复算值与 RFC 7677 常量逐字节比对(不一致即中止)。
# 用法:python3 gen_fixtures.py(幂等重写)。
import os
import struct
import hashlib
import hmac as hmac_mod
import base64

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

# ══════════════════════════════════════════════════════════════════
# P5-D 夹具(tests/db/replay_scram/):SCRAM / 扩展查询 / 事务 / 取消
# ══════════════════════════════════════════════════════════════════

OUT2 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "replay_scram")
os.makedirs(OUT2, exist_ok=True)

# ---- SCRAM 密码链(hashlib 复算;RFC 5802 §5.1)----

def scram_chain(password: str, cfirst_bare: str, sfirst: str, cfwp: str, salt_b64: str, iters: int):
    """返回 (salted_b64, proof_b64, serversig_b64);AuthMessage = 三段逗号拼接"""
    salt = base64.b64decode(salt_b64)
    authmsg = cfirst_bare + "," + sfirst + "," + cfwp
    salted = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, iters, 32)
    ckey = hmac_mod.new(salted, b"Client Key", hashlib.sha256).digest()
    stored = hashlib.sha256(ckey).digest()
    csig = hmac_mod.new(stored, authmsg.encode(), hashlib.sha256).digest()
    proof = bytes(a ^ b for a, b in zip(ckey, csig))
    srvk = hmac_mod.new(salted, b"Server Key", hashlib.sha256).digest()
    ssig = hmac_mod.new(srvk, authmsg.encode(), hashlib.sha256).digest()
    return (base64.b64encode(salted).decode(),
            base64.b64encode(proof).decode(),
            base64.b64encode(ssig).decode())

# RFC 7677 §3 常量(生成器自检锚)
RFC_USER, RFC_PASS = "user", "pencil"
RFC_NONCE = "rOprNGfwEbeRWgbNEkqO"
RFC_SERVER_NONCE = RFC_NONCE + "%hvYDpWUa2RaTCAfuxFIlj)hNlF$k0"
RFC_SALT = "W22ZaJ0SNY7soEsUEjb6gQ=="
RFC_CFIRST = "n=user,r=" + RFC_NONCE
RFC_SFIRST = "r=" + RFC_SERVER_NONCE + ",s=" + RFC_SALT + ",i=4096"
RFC_CFWP = "c=biws,r=" + RFC_SERVER_NONCE
RFC_PROOF = "dHzbZapWIk4jUhN+Ute9ytag9zjfMHgsqmmiz7AndVQ="
RFC_V = "6rriTRBi23WpRR/wtup+mMhUZUn/dB5nLTJRsjl95G4="

_, proof4096, v4096 = scram_chain(RFC_PASS, RFC_CFIRST, RFC_SFIRST, RFC_CFWP, RFC_SALT, 4096)
assert proof4096 == RFC_PROOF, proof4096
assert v4096 == RFC_V, v4096

# interp 臂低迭代改制(同链 i=1)
SFIRST_I1 = "r=" + RFC_SERVER_NONCE + ",s=" + RFC_SALT + ",i=1"
_, PROOF_I1, V_I1 = scram_chain(RFC_PASS, RFC_CFIRST, SFIRST_I1, RFC_CFWP, RFC_SALT, 1)
_, PROOF_I1_BADPW, _ = scram_chain("penc1l", RFC_CFIRST, SFIRST_I1, RFC_CFWP, RFC_SALT, 1)
CFINAL_I1 = RFC_CFWP + ",p=" + PROOF_I1
CFINAL_I1_BADPW = RFC_CFWP + ",p=" + PROOF_I1_BADPW

def sasl_mech_frame(mechs: list) -> str:
    """AuthenticationSASL:u32(10) + 机制 cstring 序 + 空串终止"""
    p = u32(10)
    for m in mechs:
        p += cstr(m)
    p += b"\x00"
    return frame(b"R", p)

def auth_extra(kind: int, extra: bytes) -> str:
    return frame(b"R", u32(kind) + extra)

# ---- scram_ok:i=1 改制全链 ok 路(R10→R11→R12→R0→K→S→Z idle) ----
write_script(os.path.join(OUT2, "scram_ok.script"), [
    "SCRAM-SHA-256 happy path (P5-D; gen_fixtures.py; RFC 7677 adapted i=1)",
    "chain: n=user / pencil / nonce " + RFC_NONCE + " / salt " + RFC_SALT + " / i=1",
    "seq: R10(SCRAM-SHA-256) R11(server-first) R12(v=) R0(AuthOk) K S Z(idle)",
    "cfinal expect: " + CFINAL_I1,
], [
    sasl_mech_frame(["SCRAM-SHA-256"]),
    auth_extra(11, SFIRST_I1.encode()),
    auth_extra(12, ("v=" + V_I1).encode()),
    auth(0),
    backend_keydata(4711, 305419896),
    param_status("client_encoding", "UTF8"),
    ready("I"),
])

# ---- scram_badpw:错口令路径(client-final 计出面 server E 28P01) ----
write_script(os.path.join(OUT2, "scram_badpw.script"), [
    "SCRAM wrong-password path (P5-D): server rejects after client-final",
    "client side computes proof with 'penc1l'; server answers E(28P01) + Z(E)",
    "cfinal(wrong pw) expect: " + CFINAL_I1_BADPW,
], [
    sasl_mech_frame(["SCRAM-SHA-256"]),
    auth_extra(11, SFIRST_I1.encode()),
    error_response({
        "S": "FATAL",
        "V": "FATAL",
        "C": "28P01",
        "M": "password authentication failed for user \"user\"",
    }),
    ready("E"),
])

# ---- scram_badsig:server-final v= 与本地 ServerSignature 不符 ----
write_script(os.path.join(OUT2, "scram_badsig.script"), [
    "SCRAM server-final signature mismatch (P5-D): v != HMAC(ServerKey, AuthMessage)",
    "walker must fail clean (err 4 auth) with sigok=0",
], [
    sasl_mech_frame(["SCRAM-SHA-256"]),
    auth_extra(11, SFIRST_I1.encode()),
    auth_extra(12, ("v=" + "A" * 43 + "=").encode()),
])

# ---- scram_nonce:combined nonce 不以 client nonce 为前缀 ----
write_script(os.path.join(OUT2, "scram_nonce.script"), [
    "SCRAM server nonce prefix violation (P5-D): r= does not extend client nonce",
    "walker must fail clean (err 4 auth)",
], [
    sasl_mech_frame(["SCRAM-SHA-256"]),
    auth_extra(11, ("r=ZZZunrelated,s=" + RFC_SALT + ",i=1").encode()),
])

# ---- scram_plus:server 仅荐 SCRAM-SHA-256-PLUS(channel binding) ----
write_script(os.path.join(OUT2, "scram_plus.script"), [
    "SCRAM plus-only mechanism list (P5-D): channel-binding-plus unsupported (registered)",
    "walker must fail clean (err 4 auth), never fall back to plain silently",
], [
    sasl_mech_frame(["SCRAM-SHA-256-PLUS"]),
])

# ---- scram_rfc7677:RFC 7677 §3 原例(i=4096;x_ emit 臂专面) ----
write_script(os.path.join(OUT2, "scram_rfc7677.script"), [
    "SCRAM RFC 7677 exact vector (P5-D; emit arm only - interp heap budget)",
    "cfinal expect: " + RFC_CFWP + ",p=" + RFC_PROOF,
    "server-final expect: v=" + RFC_V,
], [
    sasl_mech_frame(["SCRAM-SHA-256"]),
    auth_extra(11, RFC_SFIRST.encode()),
    auth_extra(12, ("v=" + RFC_V).encode()),
    auth(0),
    backend_keydata(4711, 305419896),
    ready("I"),
])

# ---- ext_query:PQexecParams 形响应流(P+B+Describe portal+E+S) ----
write_script(os.path.join(OUT2, "ext_query.script"), [
    "extended query response stream (P5-D): ParseComplete/BindComplete skipped",
    "send side (asserted by builders): P(q1,SELECT $1,$2,[23,25]) B D(P) E S",
    "seq: 1 2 T(id,name) D D C(SELECT 2) Z(idle)",
], [
    frame(b"1", b""),
    frame(b"2", b""),
    rowdesc([("id", 23), ("name", 25)]),
    datarow([b"1", b"alice"]),
    datarow([b"2", b"bob"]),
    command_complete("SELECT 2"),
    ready("I"),
])

# ---- ext_describe:语句级 Describe(ParameterDescription + RowDescription) ----
write_script(os.path.join(OUT2, "ext_describe.script"), [
    "statement-level Describe response (P5-D): t(param oids) + T, zero rows",
    "seq: 1 t(n=2,oids 23/25) T(v) Z(idle)",
], [
    frame(b"1", b""),
    frame(b"t", u16(2) + u32(23) + u32(25)),
    rowdesc([("v", 25)]),
    ready("I"),
])

# ---- tx_cycle:事务状态字节迁移 idle→T→T→idle ----
write_script(os.path.join(OUT2, "tx_cycle.script"), [
    "transaction status transitions (P5-D): ReadyForQuery I->T->T->I",
    "client side: BEGIN / SELECT 1 / COMMIT via Query; cursor chains via next",
    "seq: C(BEGIN) Z(T) C(SELECT 1) Z(T) C(COMMIT) Z(I)",
], [
    command_complete("BEGIN"),
    ready("T"),
    command_complete("SELECT 1"),
    ready("T"),
    command_complete("COMMIT"),
    ready("I"),
])

# ---- tx_error_drain:E 收口 + 尾随 Z('E')排空 + ROLLBACK 复位 ----
write_script(os.path.join(OUT2, "tx_error_drain.script"), [
    "error drain + reset-via-rollback (P5-D): E(25P02) leaves trailing Z('E')",
    "walker stops before Z (next=drain pos); drain then ROLLBACK sees Z('I')",
    "seq: C(BEGIN) Z(T) E(25P02) Z(E) C(ROLLBACK) Z(I)",
], [
    command_complete("BEGIN"),
    ready("T"),
    error_response({
        "S": "ERROR",
        "V": "ERROR",
        "C": "25P02",
        "M": "current transaction is aborted, commands ignored until end of transaction block",
    }),
    ready("E"),
    command_complete("ROLLBACK"),
    ready("I"),
])

# ---- tx_cancel:查询取消传播(E 57014 后 Z idle;连接排空后可复位) ----
write_script(os.path.join(OUT2, "tx_cancel.script"), [
    "cancel propagation (P5-D): in-flight query aborted mid-rowset, E(57014)+Z",
    "client: CancelRequest(pid,key) on separate connection; this stream shows",
    "the partial rowset then the cancel ErrorResponse then ReadyForQuery",
    "seq: T(count) D D E(57014) Z(I)",
], [
    rowdesc([("count", 23)]),
    datarow([b"1"]),
    datarow([b"2"]),
    error_response({
        "S": "ERROR",
        "V": "ERROR",
        "C": "57014",
        "M": "canceling statement due to user request",
    }),
    ready("I"),
])

# ---- tx_cancel_eof:流截断(源耗尽未见 Z)→ dirty 弃用面 ----
write_script(os.path.join(OUT2, "tx_cancel_eof.script"), [
    "stream cut mid-rowset (P5-D): script exhausted before C/Z -> eof err 3",
    "pg_conn_dirty(3) = dirty (discard; never reset/reuse across lost stream)",
    "seq: T(count) D [cut]",
], [
    rowdesc([("count", 23)]),
    datarow([b"1"]),
])

# ── P5-D 生成器自检 ──
assert scram_chain(RFC_PASS, RFC_CFIRST, RFC_SFIRST, RFC_CFWP, RFC_SALT, 4096)[1] == RFC_PROOF
print("gen p5-d ok:", OUT2)
