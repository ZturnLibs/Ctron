#!/bin/sh
# ladder.sh —— Ctron 自举验收阶梯(selfhosted 本地验收,一次跑完全部)
#
# 用法: ./ladder.sh [--full]
#   默认:  直接管线黄金对照(input_cc*/负例)
#          自编译检查阶梯(parse+sem 自身与互检源码,decl 计数对照)
#          复现解析(repro/,StructLit 误触发守卫回归)
#   --full: 追加全深度自译化(cc 解释 cc 解释 input_cc,约 4 分钟,负载高时更久)
#
# 依赖: 宿主 seed compiler-c/build/ctronc(仅作 Ctron 解释器;自举完成后可自替换)
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$DIR")
HOST="$ROOT/compiler-c/build/ctronc"
EXP="$DIR/expected"
T=$(mktemp -d /tmp/ctron_ladder.XXXXXX)


pass=0; fail=0
ok()  { pass=$((pass+1)); echo "ok  - $1"; }
bad() { fail=$((fail+1)); echo "FAIL- $1"; }

[ -x "$HOST" ] || { echo "ladder: 缺少宿主 seed $HOST(先: make -C compiler-c)" >&2; exit 2; }

# make_runner <输出模块> <输入锚路径> —— 生成换靶后的 cc 实例
make_runner() {
    sed "s|ANCHORINPUT|$2|" "$DIR/cc.ct" > "$1"
}

echo "== 1) 直接管线黄金对照 =="
for f in input_cc input_cc2 input_cc3; do
    make_runner "$T/run_$f.ct" "$DIR/$f.ct"
    ( cd "$ROOT/compiler-c" && timeout 120 ./build/ctronc run "$T/run_$f.ct" > "$T/$f.got" 2>&1 )
    if diff -q "$EXP/$f.out" "$T/$f.got" > /dev/null 2>&1; then
        ok "管线 $f 逐字一致"
    else
        bad "管线 $f 输出分歧"
    fi
done
make_runner "$T/run_neg.ct" "$DIR/input_cc_neg.ct"
( cd "$ROOT/compiler-c" && timeout 120 ./build/ctronc run "$T/run_neg.ct" > "$T/neg.got" 2>&1 )
if grep -q 'W8010: struct 含类引用字段(浅拷贝):Pair.b' "$T/neg.got"; then
    ok "负例 input_cc_neg 编译期拦截(W8010)"
else
    bad "负例 input_cc_neg 未拦截"
fi

echo "== 2) 自编译检查阶梯(parse+sem,decl 计数对照 C 解析器) =="
# cc.ct 的 selfcheck 插桩点: 在 find_decl 行前插入 decl 清单打印,并以 return 0 截止(不运行程序)
SELFMARK='var d = find_decl(file, "main")'
count_decls() {  # count_decls <源.ct> — 打印 decl 清单(每行: 序号 Tag 名)
    ln=$(grep -n 'var sems = sem_walk2(file)' "$DIR/cc.ct" | head -1 | cut -d: -f1)
    fd=$(grep -n 'var d = find_decl(file, "main")' "$DIR/cc.ct" | head -1 | cut -d: -f1)
    { head -n $((ln-1)) "$DIR/cc.ct"
      echo '            var dbgs = ""'
      echo '            var di: I32 = 1'
      echo '            while di < file.len {'
      echo '                var d = file[di]'
      echo '                dbgs = dbgs + di.to_string() + " " + d[0] + " " + d[1] + "\n"'
      echo '                di += 1'
      echo '            }'
      echo '            print(dbgs)'
      echo '            return 0'
      tail -n +$((fd+1)) "$DIR/cc.ct"
    } | sed "s|\.\./selfhosted/input_cc\.ct|$1|" > "$2"
}
check_decl() {  # check_decl <源.ct> <期望decl数> <名>
    count_decls "$1" "$T/dm_$3.ct"
    ( cd "$ROOT/compiler-c" && timeout 300 ./build/ctronc run "$T/dm_$3.ct" > "$T/dm_$3.out" 2>&1 )
    got=$(grep -c ' Fn ' "$T/dm_$3.out" 2>/dev/null || true)
    if [ "$got" = "$2" ]; then ok "解析 $3 decls=$got"; else bad "解析 $3 decls=$got 期望 $2"; fi
}
check_decl "$DIR/input_cc3.ct"   10 input_cc3
check_decl "$DIR/sem_chk.ct"    109 sem_chk
check_decl "$DIR/parsetree.ct"   57 parsetree
check_decl "$DIR/ev2.ct"        106 ev2
check_decl "$DIR/cc.ct"         158 cc

echo "== 3) 复现解析(StructLit 误触发守卫回归) =="
check_decl "$DIR/repro/s6.ct"      2 repro-s6
check_decl "$DIR/repro/hand2.ct"   2 repro-hand2
check_decl "$DIR/repro/cm_ol.ct"   2 repro-cm_ol

echo "== 4) 全深度自译化(--full) =="
if [ "${1:-}" = "--full" ]; then
    make_runner "$T/self.ct" "$DIR/cc.ct"
    s=$(date +%s)
    ( cd "$ROOT/compiler-c" && timeout 900 ./build/ctronc run "$T/self.ct" > "$T/selfdeep.got" 2>&1 )
    rc=$?
    e=$(date +%s)
    if [ $rc = 0 ] && diff -q "$EXP/input_cc.out" "$T/selfdeep.got" > /dev/null 2>&1; then
        ok "全深度自译化(cc 解释 cc 解释 input_cc)逐字一致($((e-s))s)"
    else
        bad "全深度自译化失败(rc=$rc, $((e-s))s)"
    fi
fi

echo "== 阶梯结果: pass=$pass fail=$fail =="
[ "$fail" = 0 ]
