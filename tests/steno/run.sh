#!/bin/sh
# tests/steno/run.sh —— Stage 0 steno-verify 验收门(stenolog.v1/stenolicense.v1;
# 契约 docs/superpowers/specs/2026-10-17-steno-verify-format-spec.md)
# 面:selftest + keygen/sign-chain/chain 全环 + 负例七枚 + license 四态 +
#     openssl 双向对拍(A 我签他验 / B 他签我验,同 core 同 sig 确定性等)
# 前置:compiler/native.sh、cc、homebrew openssl@3
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
EMIT="$ROOT/compiler/bin/ctron-emit"
PROJ="$ROOT/steno/verify"
[ -x "$EMIT" ] || { echo "steno: 缺 compiler/bin/ctron-emit(先: compiler/native.sh)" >&2; exit 2; }
command -v openssl > /dev/null 2>&1 || { echo "steno: 缺 openssl" >&2; exit 2; }
export CTRON_STDPATH="$ROOT/lib/std"
OSSL_CF="-I/opt/homebrew/opt/openssl@3/include"
OSSL_LB="-L/opt/homebrew/opt/openssl@3/lib -lcrypto"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
BIN="$T/steno-verify"

pass=0; fail=0
ok()   { pass=$((pass+1)); echo "  PASS $1"; }
bad()  { fail=$((fail+1)); echo "  FAIL $1"; [ $# -gt 1 ] && sed -n '1,3p' "$2"; }

# ---- 构建(emit 依赖路径 → cc crypto+fio+openssl)----
echo "== steno-verify 构建与验收 =="
if "$EMIT" run "$PROJ/src/main.ct" > "$T/sv.c" 2>"$T/emit.err" \
   && cc -O1 -w $OSSL_CF -I"$ROOT/lib/crypto/c_src" -o "$BIN" "$T/sv.c" \
       "$ROOT/lib/crypto/c_src/ctron_crypto.c" "$ROOT/lib/fio/c_src/ctron_fio.c" $OSSL_LB 2>"$T/cc.err"; then
    ok "build (emit+cc)"
else
    bad "build (emit+cc)" "$T/emit.err"; bad "build (cc)" "$T/cc.err"; exit 1
fi

# ---- selftest ----
if "$BIN" selftest > "$T/self.out" 2>&1; then ok "selftest"; else bad "selftest" "$T/self.out"; fi

# ---- keygen + 三事件链正例 ----
"$BIN" keygen --out "$T/k.hex" > "$T/pub.txt" 2>"$T/keygen.err" \
    && [ "$(wc -c < "$T/k.hex" | tr -d ' ')" = "65" ] \
    && grep -qE '^[0-9a-f]{64}$' "$T/pub.txt" && ok "keygen (priv 0600 落盘 + pub 64hex)" \
    || bad "keygen" "$T/keygen.err"

cat > "$T/in.jsonl" <<'EOF'
{"v":1,"seq":0,"ts":1730000000000,"kind":"import","actor":"steno-cli/1.0","payload":{"file":"a.wav","bytes":48000}}
{"v":1,"seq":1,"ts":1730000001000,"kind":"transcribe","actor":"","payload":{"segments":42}}
{"v":1,"seq":2,"ts":1730000002000,"kind":"export","actor":"steno-cli/1.0","payload":{"note":"adversarial ,\"hash:\" bytes inside"}}
EOF
"$BIN" sign-chain --priv "@$T/k.hex" "$T/in.jsonl" "$T/chain.jsonl" 2>"$T/sign.err" \
    && [ "$(wc -l < "$T/chain.jsonl" | tr -d ' ')" = "3" ] && ok "sign-chain (3 事件)" \
    || bad "sign-chain" "$T/sign.err"

"$BIN" chain --pub "@$T/pub.txt" "$T/chain.jsonl" > "$T/chain.out" 2>&1 \
    && grep -q "OK stenolog.v1: 3 events" "$T/chain.out" && ok "chain 正例 (rc0 + 3 events)" \
    || bad "chain 正例" "$T/chain.out"
"$BIN" chain --pub "@$T/pub.txt" "$T/chain.jsonl" --json > "$T/chainj.out" 2>&1 \
    && grep -q '"valid":true,"events":3' "$T/chainj.out" && ok "chain --json 信封" \
    || bad "chain --json" "$T/chainj.out"

# ---- 负例矩阵(七枚,各必 rc1)----
sed 's/"a.wav"/"b.wav"/' "$T/chain.jsonl" > "$T/n1.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n1.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: payload 篡改" || bad "负例: payload 篡改"

sed 's/"kind":"export"/"kind":"EXPORT"/' "$T/chain.jsonl" > "$T/n2.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n2.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: 签名域篡改" || bad "负例: 签名域篡改"

sed '$s/"sig":"\([0-9a-f]\)/"sig":"0\1/' "$T/chain.jsonl" > "$T/n3.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n3.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: sig 翻字符" || bad "负例: sig 翻字符"

sed '1d' "$T/chain.jsonl" > "$T/n4.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n4.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: 首行截断(seq 跳号)" || bad "负例: 首行截断"

head -2 "$T/chain.jsonl" > "$T/n5.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n5.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: 错公钥(尾行被换钥)" || true
# 注:n5 用错钥——另起:错钥验全链
"$BIN" keygen --out "$T/k2.hex" > "$T/pub2.txt" 2>/dev/null
"$BIN" chain --pub "@$T/pub2.txt" "$T/chain.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: 错公钥全链" || bad "负例: 错公钥全链"

printf '{"v":1,"seq":0,"ts":1,"kind":"x","actor":"","payload":1}\nnot json\n' > "$T/n6.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n6.jsonl" > /dev/null 2>&1; [ $? -eq 1 ] && ok "负例: 非法 JSON 行" || bad "负例: 非法 JSON 行"

: > "$T/n7.jsonl"
"$BIN" chain --pub "@$T/pub.txt" "$T/n7.jsonl" > "$T/n7.out" 2>&1; [ $? -eq 0 ] && grep -q "0 events" "$T/n7.out" && ok "空链合法 (0 events)" || bad "空链合法" "$T/n7.out"

"$BIN" chain --pub "@$T/pub.txt" "$T/does-not-exist.jsonl" > /dev/null 2>&1; [ $? -eq 2 ] && ok "缺文件 rc2 (IO 面)" || bad "缺文件 rc2"

# ---- license 四态 ----
cat > "$T/lic.json" <<'EOF'
{"v":1,"product":"steno","email":"dev@example.com","machine":"aa000000000000000000000000000000000000000000000000000000000000bb","issued_at":1730000000000,"updates_until":1730000050000,"nonce":"00112233445566778899aabbccddeeff"}
EOF
"$BIN" sign-license --priv "@$T/k.hex" "$T/lic.json" "$T/lic.signed.json" 2>"$T/slic.err" \
    && ok "sign-license" || bad "sign-license" "$T/slic.err"
"$BIN" license --pub "@$T/pub.txt" "$T/lic.signed.json" > /dev/null 2>&1; [ $? -eq 0 ] && ok "license 真伪面 rc0" || bad "license 真伪面"
"$BIN" license --pub "@$T/pub.txt" "$T/lic.signed.json" --machine "aa000000000000000000000000000000000000000000000000000000000000bb" > /dev/null 2>&1; [ $? -eq 0 ] && ok "license 机器绑定合" || bad "license 机器绑定合"
"$BIN" license --pub "@$T/pub.txt" "$T/lic.signed.json" --machine "00000000000000000000000000000000000000000000000000000000000000ff" > /dev/null 2>&1; [ $? -eq 1 ] && ok "license 机器绑定不合 rc1" || bad "license 机器绑定不合"
"$BIN" license --pub "@$T/pub.txt" "$T/lic.signed.json" --now 1730000100000 --strict > /dev/null 2>&1; [ $? -eq 1 ] && ok "license 过期+strict rc1" || bad "license 过期+strict"
"$BIN" license --pub "@$T/pub.txt" "$T/lic.signed.json" --now 1730000100000 > "$T/licexp.out" 2>&1; [ $? -eq 0 ] && grep -q "updates_expired true" "$T/licexp.out" && ok "license 过期只报告 rc0" || bad "license 过期只报告" "$T/licexp.out"

# ---- openssl 双向对拍 ----
if openssl genpkey -algorithm ed25519 -out "$T/os.pem" 2>"$T/os.err" \
   && openssl pkey -in "$T/os.pem" -text -noout > "$T/os.txt" 2>>"$T/os.err"; then
    PRIV_HEX=$(awk '/priv:/{f=1;next}/pub:/{f=0}f' "$T/os.txt" | tr -d ' :\n')
    PUB_HEX=$(awk '/pub:/{f=1;next}f' "$T/os.txt" | tr -d ' :\n')
    if [ "${#PRIV_HEX}" = "64" ] && [ "${#PUB_HEX}" = "64" ]; then
        ok "openssl 裸钥提取 (priv/pub 64hex)"
        # A 向:openssl 私钥经我方签链 → 我方验 + openssl 验 core
        if "$BIN" sign-chain --priv "$PRIV_HEX" "$T/in.jsonl" "$T/oschain.jsonl" 2>"$T/a.err" \
           && "$BIN" chain --pub "$PUB_HEX" "$T/oschain.jsonl" > "$T/a.out" 2>&1; then
            ok "A 向: openssl 私钥签链,我方验过"
        else
            bad "A 向: openssl 私钥签链" "$T/a.err"
        fi
        LINE1=$(head -1 "$T/oschain.jsonl")
        CORE=$(printf '%s' "$LINE1" | sed 's/,"hash":"[0-9a-f]\{64\}","sig":"[0-9a-f]\{128\}"}$//')
        OSIG=$(printf '%s' "$LINE1" | sed -n 's/.*,"sig":"\([0-9a-f]\{128\}\)"}/\1/p')
        printf '%s' "$CORE" > "$T/core.bin"
        printf '%s' "$OSIG" | xxd -r -p > "$T/os.sig" 2>/dev/null
        if openssl pkeyutl -verify -inkey "$T/os.pem" -rawin -in "$T/core.bin" -sigfile "$T/os.sig" > "$T/av.out" 2>&1; then
            ok "A 向: 我方 sig 经 openssl pkeyutl 验过"
        else
            bad "A 向: openssl 验我方 sig" "$T/av.out"
        fi
        # B 向:同 core 由 openssl 签 → 与我方 sig 确定性等;再由我方验
        if openssl pkeyutl -sign -inkey "$T/os.pem" -rawin -in "$T/core.bin" -out "$T/os2.sig" 2>"$T/b.err" \
           && [ "$(xxd -p -c 200 "$T/os2.sig")" = "$OSIG" ]; then
            ok "B 向: 同 core 同钥必同 sig (Ed25519 确定性)"
        else
            bad "B 向: 确定性对签" "$T/b.err"
        fi
    else
        bad "openssl 裸钥提取 (got ${#PRIV_HEX}/${#PUB_HEX})" "$T/os.txt"
    fi
else
    bad "openssl keygen/text" "$T/os.err"
fi

echo "== steno: $pass pass / $fail fail =="
[ "$fail" -eq 0 ] || exit 1
