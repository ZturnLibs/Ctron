#!/bin/sh
# tests/bare/run.sh —— bare 档验收门(T40 §9.3)
#
# 门序:
#   ① ctc.sh targets 注册表面(bare 双靶在册)
#   ② 未注册 target 清晰诊断(fail-closed rc=2)
#   ③ 无工具链时清晰诊断(通道指引;SKIP 语义——CI 无靶环境绿)
#   ④ ARM(qemu netduinoplus2/半主机)裸机 hello 真跑:输出断言
#   ⑤ RISC-V(qemu virt)裸机 hello 真跑:输出断言
#   ⑥ 体积报告(ELF 字节数;T42 100KB 门禁的数据源)
#   ⑦ 08_bare 转正(T41:显式 arena 值族+Region/Pool/Static 双靶真跑,成功锚断言)
#   ⑧ Pool 用尽负锚(T41:定长对象池超容量 = 裸机 panic 真跑,红绿可演示)
#
# 工具链解析随 ctc.sh(env → 宿主 clang+ld.lld → docker ctron-bare-tools);
# qemu 同三通道(宿主 qemu-system-* → docker 镜像内 qemu)。
# 全缺 = 打 SKIP 摘要 exit 0(环境登记,非门红);有工具链则门红不豁免。
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$DIR")/.." && pwd)
CTC="$ROOT/compiler/ctc.sh"
T=$(mktemp -d /tmp/ctron_bare_gate.XXXXXX)
trap 'rm -rf "$T"' EXIT
PASS=0; FAIL=0; SKIP=0

ok()   { PASS=$((PASS+1)); echo "ok $1 - $2"; }
bad()  { FAIL=$((FAIL+1)); echo "BAD $1 - $2"; }
skp()  { SKIP=$((SKIP+1)); echo "SKIP $1 - $2"; }

run_to() { # run_to <secs> <cmd...>:perl alarm 超时兜底(macOS 无 timeout)
    # alarm shift:弹出时长参,exec @ARGV 才是纯命令(此前 60 一并 exec → 找程序"60"落空)
    local secs=$1; shift
    perl -e 'alarm shift; exec @ARGV' "$secs" "$@"
}

# qemu 三通道:宿主 → docker(镜像内 qemu + ROOT 挂载)
QEMU_ARM=""; QEMU_RV=""
qemu_host() { command -v "$1" >/dev/null 2>&1 && command -v "$1"; }
qemu_docker() { # qemu_docker <qemu-bin> <args...>(ELF 路径须为 ROOT 相对)
    docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest "$@"
}

# bare_run_q <tgv> <elf> —— ⑦⑧ 共用 qemu 双通道取输出(宿主 qemu → docker);
# 通道全缺 = 输出 __noqemu__ 哨兵(环境登记 SKIP 语义)。
# 坑位:机参(QARG)按靶独立于宿主 qemu 在否——docker 通道同样需要
#(旧 ⑦ docker 面曾随宿主探测置空机参,仅 grep exhausted 掩成假绿;
#  T41 成功锚断言后暴露,随批修)
bare_run_q() {
    local tgv=$1 elf=$2
    local Q="" QARG="" QBIN=""
    if [ "$tgv" = "thumbv7em-none-eabi" ]; then
        QARG="-M netduinoplus2"
        command -v qemu-system-arm >/dev/null 2>&1 && Q=$(command -v qemu-system-arm)
    else
        QARG="-M virt -bios none"
        command -v qemu-system-riscv32 >/dev/null 2>&1 && Q=$(command -v qemu-system-riscv32)
    fi
    if [ -n "$Q" ]; then
        run_to 30 "$Q" $QARG -nographic -monitor none -semihosting-config enable=on,target=native -kernel "$elf" 2>&1 || true
        return 0
    fi
    if docker image inspect ctron-bare-tools:latest >/dev/null 2>&1; then
        local DT="$ROOT/.cache/bare/gate_$$"; mkdir -p "$DT"
        cp "$elf" "$DT/hello.elf"
        QBIN="qemu-system-arm"; [ "$tgv" = "riscv32imac-unknown-none" ] && QBIN="qemu-system-riscv32"
        run_to 60 docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest sh -c "timeout 20 $QBIN $QARG -nographic -monitor none -semihosting-config enable=on,target=native -kernel .cache/bare/gate_$$/hello.elf" 2>&1 || true
        rm -rf "$DT"
        return 0
    fi
    echo "__noqemu__"
}

echo "== ① 注册表面 =="
if "$CTC" targets 2>&1 | grep -q "thumbv7em-none-eabi" && "$CTC" targets 2>&1 | grep -q "riscv32imac-unknown-none"; then
    ok 1 "bare 双靶在册"
else
    bad 1 "targets 表缺 bare 双靶"
fi

echo "== ② 未注册诊断 =="
if "$CTC" build "$DIR/hello.ct" --target no-such-triple >"$T/u.out" 2>&1; then
    bad 2 "未注册 target 未拦截"
elif grep -q "未注册后端" "$T/u.out"; then
    ok 2 "未注册诊断(rc≠0)"
else
    bad 2 "未注册诊断文案缺失: $(head -1 "$T/u.out")"
fi

echo "== ③④⑤ bare 真跑 =="
ARMElf="$T/hello_arm.elf"
RVElf="$T/hello_rv.elf"
ARMOK=1; RVOK=1
"$CTC" build "$DIR/hello.ct" --target thumbv7em-none-eabi -o "$ARMElf" >"$T/b_arm.out" 2>&1 || ARMOK=0
"$CTC" build "$DIR/hello.ct" --target riscv32imac-unknown-none -o "$RVElf" >"$T/b_rv.out" 2>&1 || RVOK=0

if [ "$ARMOK" = "1" ]; then
    ARMSZ=$(wc -c < "$ARMElf" | tr -d ' ')
    ok 3 "ARM ELF 构建($ARMSZ 字节;T42 门数据源)"
    if QA=$(qemu_host qemu-system-arm); then
        OUT=$(run_to 30 "$QA" -M netduinoplus2 -nographic -monitor none -semihosting-config enable=on,target=native -kernel "$ARMElf" 2>&1 || true)
    elif docker image inspect ctron-bare-tools:latest >/dev/null 2>&1; then
        # docker 通道:ELF 拷入 ROOT 临时位,跑完即清
        DT="$ROOT/.cache/bare/gate_$$"; mkdir -p "$DT"
        cp "$ARMElf" "$DT/hello.elf"
        OUT=$(run_to 60 docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest qemu-system-arm -M netduinoplus2 -nographic -monitor none -semihosting-config enable=on,target=native -kernel ".cache/bare/gate_$$/hello.elf" 2>&1 || true)
        rm -rf "$DT"
    else
        OUT=""; skp 4 "qemu 不在(宿主无 qemu-system-arm 且无 docker 镜像)"
    fi
    if [ -n "$OUT" ]; then
        if printf '%s' "$OUT" | grep -q "hello bare"; then
            ok 4 "ARM 裸机 hello(semihosting 输出断言)"
        else
            bad 4 "ARM 输出无锚文本: $(printf '%s' "$OUT" | head -2 | tr '\n' '|')"
        fi
    else
        bad 4 "ARM qemu 无输出(挂起/超时/段错误;qrc 探针缺失)"
    fi
else
    if grep -q "工具链不可用" "$T/b_arm.out"; then
        skp 3 "ARM 通道工具链缺(诊断在册)——$(grep -o '工具链不可用.*' "$T/b_arm.out" | head -c 80)"
    else
        bad 3 "ARM 构建失败: $(tail -2 "$T/b_arm.out" | tr '\n' '|')"
    fi
fi

if [ "$RVOK" = "1" ]; then
    RVSZ=$(wc -c < "$RVElf" | tr -d ' ')
    ok 5 "RISC-V ELF 构建($RVSZ 字节)"
    if QR=$(qemu_host qemu-system-riscv32); then
        OUT=$(run_to 30 "$QR" -M virt -bios none -nographic -monitor none -semihosting-config enable=on,target=native -kernel "$RVElf" 2>&1 || true)
    elif docker image inspect ctron-bare-tools:latest >/dev/null 2>&1; then
        DT="$ROOT/.cache/bare/gate_$$"; mkdir -p "$DT"
        cp "$RVElf" "$DT/hello.elf"
        OUT=$(run_to 60 docker run --rm -v "$ROOT":/ctroot -w /ctroot ctron-bare-tools:latest qemu-system-riscv32 -M virt -bios none -nographic -monitor none -semihosting-config enable=on,target=native -kernel ".cache/bare/gate_$$/hello.elf" 2>&1 || true)
        rm -rf "$DT"
    else
        OUT=""; skp 6 "qemu 不在(宿主无 qemu-system-riscv32 且无 docker 镜像)"
    fi
    if [ -n "$OUT" ]; then
        if printf '%s' "$OUT" | grep -q "hello bare"; then
            ok 6 "RISC-V 裸机 hello(semihosting 输出断言)"
        else
            bad 6 "RISC-V 输出无锚文本: $(printf '%s' "$OUT" | head -2 | tr '\n' '|')"
        fi
    else
        bad 6 "RISC-V qemu 无输出(挂起/超时/段错误)"
    fi
else
    if grep -q "工具链不可用" "$T/b_rv.out"; then
        skp 5 "RISC-V 通道工具链缺(诊断在册)——$(grep -o '工具链不可用.*' "$T/b_rv.out" | head -c 80)"
    else
        bad 5 "RISC-V 构建失败: $(tail -2 "$T/b_rv.out" | tr '\n' '|')"
    fi
fi

echo "== ⑥ 体积门禁(T42 §9.4:bare+core < 100KB 硬指标;full hello < 1MB 目标) =="
gate_sz() { # gate_sz <路径> <上限字节> <名> —— 超限 rc=1
    local f=$1 cap=$2 nm=$3
    local sz; sz=$(wc -c < "$f" | tr -d ' ')
    if [ "$sz" -lt "$cap" ]; then
        echo "  ok  : $nm = $sz 字节 < $cap"
        return 0
    fi
    echo "  BAD : $nm = $sz 字节 ≥ $cap(§9.4 超限)" >&2
    return 1
}
# 超限样自证:假造 150KB ELF,门必须拒(红绿可演示)
OVERSAMPLE="$T/oversize.elf"
dd if=/dev/zero of="$OVERSAMPLE" bs=1024 count=150 2>/dev/null
if gate_sz "$OVERSAMPLE" 102400 "超限样(150KB 假 ELF)" 2>"$T/over.err"; then
    bad 7 "体积门未拒超限样(红绿不可演示)"
else
    ok 7 "体积门拒超限样(红绿可演示)"
fi
if [ "$ARMOK" = "1" ]; then
    if gate_sz "$ARMElf" 102400 "thumbv7em bare ELF"; then ok 8 "bare+core < 100KB 硬指标"; else bad 8 "bare 体积超 100KB(§9.4)"; fi
else
    skp 8 "bare 体积门(无 ARM ELF;工具链登记)"
fi
if [ "$RVOK" = "1" ]; then
    if gate_sz "$RVElf" 102400 "riscv32 bare ELF"; then ok 9 "riscv bare+core < 100KB 硬指标"; else bad 9 "riscv bare 体积超 100KB(§9.4)"; fi
else
    skp 9 "riscv 体积门(无 RISC-V ELF;工具链登记)"
fi
echo "== ⑦ 08_bare 转正(T41 §6.6:显式 arena 值族+Region/Pool/Static 真跑;零 GC) =="
if "$CTC" check "$ROOT/tests/08_bare.ct" >"$T/08b_chk.out" 2>&1; then
    ok 11 "08_bare check 面(Arena/Region/Pool/Static 解析+型别)"
else
    bad 11 "08_bare check 红: $(tail -1 "$T/08b_chk.out")"
fi
for tgv in thumbv7em-none-eabi riscv32imac-unknown-none; do
    B08="$T/08b_${tgv}.elf"
    if "$CTC" build "$ROOT/tests/08_bare.ct" --target "$tgv" -o "$B08" >"$T/08b_b.out" 2>&1; then
        O8=$(bare_run_q "$tgv" "$B08")
        if printf '%s' "$O8" | grep -q "__noqemu__"; then
            skp "08_bare@$tgv" "qemu 不在(环境登记)"
        elif printf '%s' "$O8" | grep -q "exhausted"; then
            bad "12-$tgv" "08_bare 裸机 panic: $(printf '%s' "$O8" | head -1)"
        elif printf '%s' "$O8" | grep -q "t41 alloc ok"; then
            ok "12-$tgv" "08_bare 裸机真跑(断言过+T41 成功锚)"
        else
            bad "12-$tgv" "08_bare 无成功锚(断言早退?): $(printf '%s' "$O8" | tail -1)"
        fi
    else
        bad "13-$tgv" "08_bare 构建失败: $(tail -1 "$T/08b_b.out")"
    fi
done

echo "== ⑧ Pool 用尽负锚(T41:定长对象池超容量 = 裸机 panic 真跑;红绿可演示) =="
for tgv in thumbv7em-none-eabi riscv32imac-unknown-none; do
    BPE="$T/pex_${tgv}.elf"
    if "$CTC" build "$DIR/pool_exhaust.ct" --target "$tgv" -o "$BPE" >"$T/pex_b.out" 2>&1; then
        OPE=$(bare_run_q "$tgv" "$BPE")
        if printf '%s' "$OPE" | grep -q "__noqemu__"; then
            skp "pool_exhaust@$tgv" "qemu 不在(环境登记)"
        elif printf '%s' "$OPE" | grep -q "pool exhausted"; then
            ok "14-$tgv" "Pool 用尽裸机 panic(负锚真跑)"
        else
            bad "14-$tgv" "Pool 用尽无 panic 输出: $(printf '%s' "$OPE" | tail -1)"
        fi
    else
        bad "14-$tgv" "pool_exhaust 构建失败: $(tail -1 "$T/pex_b.out")"
    fi
done

NATBIN="$T/hello_nat"
if "$CTC" build "$DIR/hello.ct" -o "$NATBIN" >"$T/nat.out" 2>&1; then
    if gate_sz "$NATBIN" 1048576 "full hello 产物"; then ok 10 "full < 1MB 目标"; else bad 10 "full 产物超 1MB(§9.4 目标)"; fi
else
    skp 10 "full 产物门(native 构建失败: $(tail -1 "$T/nat.out"))"
fi

echo "----------------------------------------"
echo "bare 门: $PASS 过 / $FAIL 败 / $SKIP 跳(环境登记)"
[ "$FAIL" = "0" ] || exit 1
exit 0
