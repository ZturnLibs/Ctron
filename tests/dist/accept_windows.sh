#!/bin/sh
# accept_windows.sh —— Windows 专项发布验收门禁(spec 工具链分发 §7 第 6-9 项)
#                      + P2-2 审查移交的 CI 必查:编码三向断言 / ps1 语法门 / BOM 门
#
# 运行环境:MSYS2 bash(github windows-latest + msys2/setup-msys2 MINGW64;mingw gcc 在 PATH)
# 前置(cwd = 仓库根,release workflow windows job 已就位):
#   bin/ctron-{cc,chk,emit}.exe    cc-only 构建(预发射 C 直接 gcc,绕过 seed)
#   ctron/                          zip 载荷目录:bin/{ctc.cmd,ctc.ps1,ctron-*.exe}
#                                   + lib/ctron/std + share/doc + VERSION(spec §2.2)
#   ctc                             sh 版驱动(仓库根检出件;帮助文本 conformance 基准)
#   tests/06_crlf.ct                CRLF 回归夹具(检出件,真 CRLF 字节)
#
# 断言面(失败计数,末尾非零退出):
#   A) zip 载荷布局(spec §2.2):bin 五件 + lib/ctron/std(str.ct 探针)+ VERSION
#   B) BOM 门:ctc.ps1 首三字节 = EF BB BF(PS 5.1 无 BOM 按 ANSI 解码,中文帮助乱码)
#   C) ps1 语法门:[scriptblock]::Create 全文解析(P2-2 移交:本机无 pwsh,语法验证落 CI)
#   D) 编码三向断言(P2-2 审查移交 CI 必查第一项;归一化禁止 strip 非 ASCII):
#      ① 驱动面:msys2 管道下 ctc.ps1 --help 输出按 UTF-8 断言含中文词
#        (powershell.exe 重定向 stdout 默认 OEM CP → 乱码 '?',ps1 第 5 行
#        [Console]::OutputEncoding=UTF8 即为此而设;本断言就是它的回归钉)
#      ② 发射面:含中文字面量程序经 ctc.ps1 build → 产物 .c 首字节无 BOM
#        且中文串为正确 UTF-8 字节(WriteAllLines + UTF8Encoding($false) 回归钉)
#      ③ 可执行面:zh.exe 运行输出中文不乱码(SetConsoleOutputCP(CP_UTF8) 回归钉)
#   E) 并发夹具(§7.6):spawn/Channel 有界通道经 ctc.ps1 build + 运行;
#      发射 C 含 pthread_create/join/mutex/cond,Windows 上即 winpthreads 通路。
#      取最小内联夹具:tests/06_concurrency.ct 为 test 块形态且含 for 通配模式
#      (`for _ in`),发射面命中 ct_stmt:for pat:PatWild panic(build 属 beta 能力面
#      挂账,spec §4 已记载)——本夹具取其 "channel roundtrip 0+1+4+9=14" 同语义
#      while 形,语义同源、发射面安全
#   F) 深递归夹具(§7.7,-Wl,--stack,8388608 生效验证):600 层递归 ≈ 2.8MB 宿主栈
#      (标定:BOOTSTRAP.md §7,~1800 步击穿 8MB → ~4.6KB/步)——Windows 主线程
#      默认 1MB 栈必炸,8MB 链接参数下须过;深度取 (220, 1740) 区间中段,双向留余
#   G) CRLF 夹具(§5 第 8 项回归):tests/06_crlf.ct 经 build + run,另跑解释臂
#   H) 驱动 conformance(§7.9):ctc.ps1 --help 与 sh 版归一化 diff(差异域白名单
#      映射后必须零 diff)。归一化 = 剥 CR + 折叠空白,不做任何非 ASCII 处理。
#      白名单 = P2-2 登记的刻意差异共三处:① CC 默认 cc→gcc(usage 一处);
#      ② 产物 <stem>.exe / build/<name>.exe(build 详助两处);③ build 前置行
#      mingw-w64(MSYS2 或 w64devkit)vs 本机 C 编译器——任务书"两处"为简记,
#      ③ 为 P2-2 命令面对照表登记在案的第三处
#   I) rc 矩阵经 ctc.cmd 垫片(用户入口全链:cmd → powershell → ctron-*):
#      --version rc=0 且 = VERSION 文件;run 负例诊断 rc=1;未知子命令 rc=2;
#      无 cc build rc=2 且 .c 已产出(CC=/nonexistent/cc 法,同 accept.sh)
#
# 已知边界(语义注明,不在断言面):W8901 整库缺失盲区(安装损坏到 str.ct 探针
# 自身失效时静默回落 E2020,语义同 accept.sh 注);Windows zip 的 share/doc 本
# 任务书组装步为空,examples 面不入 Windows 断言;E5030 碰撞与 std 缺失告警面
# 同 accept.sh 姿态不入(见其头注)。
set -u

pass=0; fail=0
ok()  { echo "  ok  : $1"; pass=$((pass+1)); }
bad() { echo "  FAIL: $1"; fail=$((fail+1)); }

W=$(mktemp -d /tmp/ctron_awin.XXXXXX) || exit 2
trap 'rm -rf "$W"' EXIT

# ps1 驱动(zip 载荷,发布物本体)
ps1() { powershell -NoProfile -ExecutionPolicy Bypass -File "$PKG/bin/ctc.ps1" "$@"; }
# ctc.cmd 垫片(用户入口;cd 至同目录规避 msys2 对含反斜杠参数的转换歧义)
shim() { ( cd "$PKG/bin" && cmd //c ctc.cmd "$@" ); }

echo "== A) zip 载荷布局(spec §2.2)== "
PKG="$PWD/ctron"
if [ -f "$PKG/bin/ctc.ps1" ] && [ -f "$PKG/bin/ctc.cmd" ] \
    && [ -f "$PKG/bin/ctron-cc.exe" ] && [ -f "$PKG/bin/ctron-chk.exe" ] \
    && [ -f "$PKG/bin/ctron-emit.exe" ] && [ -f "$PKG/lib/ctron/std/str.ct" ] \
    && [ -d "$PKG/share/doc" ] && [ -f "$PKG/VERSION" ]; then
    ok "载荷齐:bin 五件 + std/str.ct + share/doc + VERSION"
else
    bad "载荷缺件(spec §2.2):$PKG"
fi

echo "== B) BOM 门(ctc.ps1 首三字节 EF BB BF)== "
bom=$(head -c 3 "$PKG/bin/ctc.ps1" | od -An -tx1 | tr -d ' \n')
if [ "$bom" = "efbbbf" ]; then
    ok "ctc.ps1 UTF-8 BOM 在(ef bb bf)"
else
    bad "ctc.ps1 首三字节 = [$bom],期望 efbbbf(BOM 被剥即 PS5.1 ANSI 乱码)"
fi

echo "== C) ps1 语法门([scriptblock]::Create)== "
rc=0
powershell -NoProfile -ExecutionPolicy Bypass -Command 'try { [scriptblock]::Create((Get-Content -LiteralPath "ctron/bin/ctc.ps1" -Raw)) | Out-Null; exit 0 } catch { $host.UI.WriteErrorLine($_.ToString()); exit 1 }' 2> "$W/pserr.txt" || rc=$?
if [ $rc -eq 0 ]; then
    ok "ctc.ps1 全文语法解析通过"
else
    bad "ctc.ps1 语法解析失败 rc=$rc:$(head -3 "$W/pserr.txt")"
fi

echo "== D) 编码三向断言(P2-2 移交;① 驱动面 UTF-8 中文词)== "
rc=0; ps1 --help > "$W/ps1help.txt" 2>&1 || rc=$?
if [ $rc -eq 0 ] && grep -q '工具链驱动' "$W/ps1help.txt" && grep -q '解释执行' "$W/ps1help.txt"; then
    ok "ps1 --help 管道输出为 UTF-8(含 工具链驱动/解释执行,未 strip 非 ASCII)"
else
    bad "ps1 --help rc=$rc 或中文失真(UTF-8 断言失败):$(head -3 "$W/ps1help.txt")"
fi

echo "== D) ② 发射面:中文程序 build → .c 无 BOM 且中文串字节正确 == "
printf 'fn main() {\n    println("中文 ok")\n}\n' > "$W/zh.ct"
rc=0; ps1 build "$W/zh.ct" > "$W/zhb.log" 2>&1 || rc=$?
zhbom=$(head -c 3 "$W/zh.c" 2>/dev/null | od -An -tx1 | tr -d ' \n')
if [ $rc -eq 0 ] && [ -f "$W/zh.c" ] && [ "$zhbom" != "efbbbf" ] && grep -q '中文 ok' "$W/zh.c"; then
    ok "zh.c 无 BOM(首字节 $zhbom)且含 UTF-8 中文串"
else
    bad "zh build rc=$rc 首字节=[$zhbom] 中文串=$([ -f "$W/zh.c" ] && grep -c '中文 ok' "$W/zh.c" || echo 无文件)"
fi

echo "== D) ③ 可执行面:zh.exe 输出中文不乱码 == "
if [ -f "$W/zh.exe" ]; then
    rc=0; ZH_OUT=$(cd "$W" && ./zh.exe 2>&1) || rc=$?
    if [ $rc -eq 0 ] && printf '%s' "$ZH_OUT" | grep -q '中文 ok'; then
        ok "zh.exe 输出 = 中文 ok(UTF-8 直达管道)"
    else
        bad "zh.exe rc=$rc out=[$ZH_OUT]"
    fi
else
    bad "zh.exe 未产出(② 发射/编译失败连带)"
fi

echo "== E) 并发夹具(spawn/Channel → winpthreads,§7.6)== "
cat > "$W/conc.ct" <<'EOF'
fn main() {
    let total = scope { |s|
        let (tx, rx) = Channel[I32](4)
        s.spawn(|| {
            var i: I32 = 0
            while i < 4 {
                tx.send(i * i).expect("send")
                i += 1
            }
        })
        var acc: I32 = 0
        var k: I32 = 0
        while k < 4 {
            acc += rx.recv().expect("recv")
            k += 1
        }
        acc
    }
    println("conc=" + total.to_string())
}
EOF
rc=0; ps1 build "$W/conc.ct" > "$W/cb.log" 2>&1 || rc=$?
if [ $rc -eq 0 ]; then
    rc=0; CONC_OUT=$(cd "$W" && ./conc.exe 2>&1) || rc=$?
    if [ $rc -eq 0 ] && [ "$CONC_OUT" = "conc=14" ]; then
        ok "spawn/Channel 夹具 build+run:conc=14(0+1+4+9)"
    else
        bad "conc.exe rc=$rc out=[$CONC_OUT]"
    fi
else
    bad "conc build rc=$rc:$(head -3 "$W/cb.log")"
fi

echo "== F) 深递归夹具(-Wl,--stack 生效,§7.7)== "
cat > "$W/deep.ct" <<'EOF'
fn down(n: I32) -> I32 {
    if n <= 0 {
        return 0
    }
    return 1 + down(n - 1)
}
fn main() {
    println("deep=" + down(600).to_string())
}
EOF
rc=0; ps1 build "$W/deep.ct" > "$W/db.log" 2>&1 || rc=$?
if [ $rc -eq 0 ]; then
    rc=0; DEEP_OUT=$(cd "$W" && ./deep.exe 2>&1) || rc=$?
    if [ $rc -eq 0 ] && [ "$DEEP_OUT" = "deep=600" ]; then
        ok "600 层深递归通过(8MB 链接栈下不段错误)"
    else
        bad "deep.exe rc=$rc out=[$DEEP_OUT](600 层 ≈2.8MB,1MB 默认栈形态必炸,8MB 参数须救回)"
    fi
else
    bad "deep build rc=$rc:$(head -3 "$W/db.log")"
fi

echo "== G) CRLF 夹具(§5.8 回归)== "
# 夹具复制进 scratch(build 产物不落入检出树;cp 保真 CRLF 字节)
cp tests/06_crlf.ct "$W/06_crlf.ct"
crlfbytes=$(head -c 200 tests/06_crlf.ct | od -An -tx1 | tr -d ' \n')
crlfnote=异常
case $crlfbytes in *0d0a*) crlfnote=确认 ;; esac
rc=0; ps1 build "$W/06_crlf.ct" > "$W/cl.log" 2>&1 || rc=$?
if [ $rc -eq 0 ]; then
    rc=0; CR_OUT=$(cd "$W" && ./06_crlf.exe 2>&1) || rc=$?
    if [ $rc -eq 0 ] && [ "$CR_OUT" = "crlf ok" ]; then
        ok "CRLF 源 build+run:crlf ok(检出件 CRLF 字节:$crlfnote)"
    else
        bad "06_crlf.exe rc=$rc out=[$CR_OUT]"
    fi
else
    bad "CRLF build rc=$rc:$(head -3 "$W/cl.log")"
fi
rc=0; CR_RUN=$(ps1 run "$W/06_crlf.ct" 2>&1) || rc=$?
if [ $rc -eq 0 ] && [ "$CR_RUN" = "crlf ok" ]; then
    ok "CRLF 源解释臂 run:crlf ok"
else
    bad "CRLF 解释臂 rc=$rc out=[$CR_RUN]"
fi

echo "== H) 驱动 conformance:ps1 vs sh 归一化 diff(§7.9)== "
# 归一化:剥 CR + 折叠空白;差异域白名单经映射消解(P2-2 登记三处,见头注 H)
norm() { tr -d '\r' | sed 's/[[:space:]][[:space:]]*/ /g; s/^ //; s/ $//'; }
map_sh() {
    sed -e 's/CC(默认 cc)/CC(默认 gcc)/' \
        -e 's/产物 <stem> 与 <stem>\.c/产物 <stem>.exe 与 <stem>.c/' \
        -e 's/产物 build\/<name>;/产物 build\/<name>.exe;/' \
        -e 's/前置:本机 C 编译器(可用 CC 覆盖)/前置:mingw-w64(MSYS2 或 w64devkit)/'
}
ps1 --help > "$W/ps1_h.txt" 2>&1
sh ctc --help 2>&1 | norm | map_sh > "$W/sh_h.txt"
norm < "$W/ps1_h.txt" > "$W/ps1_h.n"
if diff "$W/sh_h.txt" "$W/ps1_h.n" > "$W/h.diff" 2>&1; then
    ok "--help 与 sh 版归一化后一致(白名单域映射后零 diff)"
else
    bad "--help 归一化 diff 非空:$(head -5 "$W/h.diff")"
fi
ps1 build --help > "$W/ps1_bh.txt" 2>&1
sh ctc build --help 2>&1 | norm | map_sh > "$W/sh_bh.txt"
norm < "$W/ps1_bh.txt" > "$W/ps1_bh.n"
if diff "$W/sh_bh.txt" "$W/ps1_bh.n" > "$W/bh.diff" 2>&1; then
    ok "build --help 与 sh 版归一化后一致(.exe/前置行白名单域内)"
else
    bad "build --help 归一化 diff 非空:$(head -5 "$W/bh.diff")"
fi

echo "== I) rc 矩阵(经 ctc.cmd 垫片)== "
rc=0; SHIMV=$(shim --version 2>&1) || rc=$?
if [ $rc -eq 0 ] && [ "$SHIMV" = "ctron $(cat "$PKG/VERSION")" ]; then
    ok "cmd 垫片 --version = $SHIMV"
else
    bad "cmd 垫片 --version rc=$rc out=[$SHIMV] 期望=[ctron $(cat "$PKG/VERSION")]"
fi

printf 'fn main() {\n    println(nope)\n}\n' > "$W/neg.ct"
rc=0; NEG=$(shim run "$W/neg.ct" 2>&1) || rc=$?
if [ $rc -eq 1 ] && [ -n "$NEG" ]; then
    ok "run 诊断 rc=1:$(printf '%s' "$NEG" | head -1)"
else
    bad "run 负例 rc=$rc out=[$NEG]"
fi

rc=0; UNK=$(shim frobnicate 2>&1) || rc=$?
if [ $rc -eq 2 ]; then
    ok "未知子命令 rc=2"
else
    bad "未知子命令 rc=$rc(约定 2)out=[$UNK]"
fi

printf 'fn main() {\n    println("hello, ctron")\n}\n' > "$W/nc2.ct"
rc=0; NC2=$(CC=/nonexistent/gcc shim build "$W/nc2.ct" 2>&1) || rc=$?
if [ $rc -eq 2 ] && [ -f "$W/nc2.c" ]; then
    ok "无 cc build rc=2 且 nc2.c 已产出(发射先于预检)"
else
    bad "无 cc build rc=$rc .c=$([ -f "$W/nc2.c" ] && echo 有 || echo 无) out=[$NC2]"
fi

echo "accept_windows: $pass ok / $fail fail"
[ "$fail" -eq 0 ] || exit 1
