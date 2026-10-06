# T50 闭源分发 L1(`ctron pkg verify` 命令面)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 D8-2 L2 留给编排层的 L1 校验从 smoke 内联 shasum 升为用户命令面 `ctron pkg verify <工件目录>`——SHA256SUMS 逐成员重算 + self_digest 重算 + 形态 fail-closed,补齐 §7.2 离线验包路径的前半段(trace 复放腿归 S3 挂账)。

**Architecture:** 纯 shell 编排层实现,落在用户 CLI `./ctron`(与 T49 add/publish/lock 同面;ctc.sh 不进安装布局,不加面,零重复)。校验四段:①形态面(meta.ctcl/SHA256SUMS/impl/ 三件齐,§3.5 后缀即契约)②SHA256SUMS 逐行解析(畸形行/路径越界/成员缺失/摘要不符逐项点名)③self_digest 重算(sha256(SHA256SUMS 字节) vs meta artifact 块记录)④判决(rc 0/1/2)。复用 `./ctron` 既有 `sha256_file` 助手(darwin shasum / linux sha256sum 双封装)。

**Tech Stack:** POSIX sh(`./ctron`,`set -u`);smoke 腿(compiler/test/smoke.sh);工件夹具用 ctc.sh ast --ast=seal 现场封印。

## Global Constraints

- **零编译器触碰**:不改 compiler/src 任何文件;decl 锁不动;不重拼;不重建 native。
- 摘要定义冻结(D8-2,勿重设计):SHA256SUMS 行 = `<hex>␣␣<相对路径>\n`(两空格);成员 = impl/** 按路径字节序,不含 meta;self_digest = sha256(SHA256SUMS 字节),meta artifact 块内值形 `sha256:<hex>`。
- fail-closed:任何一段不过即 rc=1,不存在"带警告通过";失败逐项点名(路径 + 双摘要)。
- rc 约定(`./ctron` 头注宪法):0 成功 / 1 程序诊断失败(校验不过)/ 2 环境或用法错误。
- **路径越界拒绝**:成员路径绝对形(`/*`)或含 `..`(`*..*`)一律拒——SHA256SUMS 是不可信输入。
- S1b 缓存接线、L3 in-language sha256、跨发布者命名空间**维持各自挂账**,本批不混入(触发线不变)。
- 本批触发 = 用户点名 T50(2026-10-06,同 D8-2 L2 批先例);trace 复放腿 = §7.2 后半段,归 S3,落地注中如实登记。
- smoke 标签现状:3p/3q 各有双腿撞号(历史遗留)——新腿标 **3r**,不修旧号;插入位置 = T50 3p 腿结束之后、bare 档 3p 之前(复用 `$DG`/`$DG/bh` 已封工件,`DG="$T/dg"` 在 3p 腿首行定义)。

---

### Task 1: `./ctron` 加 `pkg verify` 命令面(三面接线 + 校验算法)

**Files:**
- Modify: `./ctron`(usage() 文本;help_cmd case 表;`--help` 拦截 case 行 282;dispatch case 在 `publish)` 臂后、`*)` 前加 `pkg)` 臂)

**Interfaces:**
- Consumes: 既有 `sha256_file()`(`./ctron` 顶部已定义);D8-2 L2 seal 产物(`ctc.sh ast --ast=seal` 的 meta.ctcl/SHA256SUMS/impl/ 布局)。
- Produces: `ctron pkg verify <artifact-dir>`;stdout 成功行 `ctron pkg verify OK: <dir>(N 成员,self_digest sha256:…)`;stderr 失败逐项行;rc 0/1/2。Task 2 smoke 腿依赖的文案串:`pkg verify OK` / `成员摘要不符: ` / `缺 SHA256SUMS` / `路径越界` / `self_digest 不符`。

- [ ] **Step 1: usage() 增一行**(在 `ctron publish` 行之后)

```sh
  ctron pkg verify <artifact-dir>
                             密封工件(.ctart)摘要自洽校验(D8-2 L1):SHA256SUMS 逐成员重算
                             + self_digest 重算;fail-closed,不过即 rc=1
```

- [ ] **Step 2: help_cmd case 表加 `pkg)` 项**(在 `lock)` 行后)

```sh
    pkg) echo "ctron pkg verify <artifact-dir> —— 密封工件摘要自洽校验(D8-2 L1):SHA256SUMS 逐成员重算 + self_digest=sha256(SHA256SUMS 字节) 重算比对;缺 meta/SHA256SUMS/impl 或任一成员不符即 fail-closed rc=1;trace 复放腿归 S3";;
```

- [ ] **Step 3: `--help` 拦截 case 加 `pkg`**(行 282 `run|check|...|lock)` 组尾追加 `|pkg`)

- [ ] **Step 4: dispatch 加 `pkg)` 臂**(`publish)` 臂 `;;` 之后、`*)` 未知子命令臂之前,完整代码):

```sh
    pkg)
        [ $# -ge 2 ] || die2 "pkg 需要子命令(现支持: pkg verify <工件目录>)"
        PV_SUB=$1; shift
        [ "$PV_SUB" = "verify" ] || die2 "pkg: 未知子命令 '$PV_SUB'(现支持: verify)"
        [ $# -ge 1 ] || die2 "pkg verify 需要工件目录(.ctart)"
        [ -d "$1" ] || die2 "pkg verify: 工件目录不存在: $1"
        PV_DIR=$1
        PV_BAD=0
        PV_N=0
        # ① 形态面(§3.5 后缀即契约:自称 .ctart 而缺成员 = 损坏/伪造,fail-closed)
        for PV_F in meta.ctcl SHA256SUMS; do
            [ -f "$PV_DIR/$PV_F" ] || { echo "ctron pkg verify: 缺 $PV_F(fail-closed 拒载)" >&2; PV_BAD=1; }
        done
        [ -d "$PV_DIR/impl" ] || { echo "ctron pkg verify: 缺 impl/(密封实现目录;fail-closed 拒载)" >&2; PV_BAD=1; }
        [ "$PV_BAD" -eq 0 ] || exit 1
        # ② SHA256SUMS 逐成员重算(路径字节序由 seal 保证,此处只对账;SHA256SUMS 是不可信输入)
        while IFS= read -r PV_LINE; do
            [ -n "$PV_LINE" ] || continue
            PV_HEX=${PV_LINE%%  *}
            PV_PATH=${PV_LINE#*  }
            if [ "$PV_HEX" = "$PV_LINE" ] || ! printf '%s' "$PV_HEX" | grep -Eq '^[0-9a-f]{64}$'; then
                echo "ctron pkg verify: SHA256SUMS 行畸形: $PV_LINE" >&2
                PV_BAD=1; continue
            fi
            case $PV_PATH in
                /*|*..*|"")
                    echo "ctron pkg verify: 成员路径越界: $PV_PATH" >&2
                    PV_BAD=1; continue ;;
            esac
            if [ ! -f "$PV_DIR/$PV_PATH" ]; then
                echo "ctron pkg verify: 成员缺失: $PV_PATH" >&2
                PV_BAD=1; continue
            fi
            PV_ACT=$(sha256_file "$PV_DIR/$PV_PATH")
            if [ "$PV_ACT" != "$PV_HEX" ]; then
                echo "ctron pkg verify: 成员摘要不符: $PV_PATH 记录 $PV_HEX 实际 $PV_ACT" >&2
                PV_BAD=1
            fi
            PV_N=$((PV_N+1))
        done < "$PV_DIR/SHA256SUMS"
        # ③ self_digest 重算 = sha256(SHA256SUMS 字节)(D8-2 冻结定义;与成员腿相互独立)
        PV_SD=$(sed -n 's/.*self_digest = "\(.*\)"/\1/p' "$PV_DIR/meta.ctcl" | head -1)
        PV_SUM=$(sha256_file "$PV_DIR/SHA256SUMS")
        if [ -z "$PV_SD" ]; then
            echo "ctron pkg verify: meta 缺 self_digest(fail-closed;工件须经 ctc.sh ast --ast=seal 摘要步骤)" >&2
            PV_BAD=1
        elif [ "$PV_SD" != "sha256:$PV_SUM" ]; then
            echo "ctron pkg verify: self_digest 不符: 记录 $PV_SD 实际 sha256:$PV_SUM" >&2
            PV_BAD=1
        fi
        # ④ 判决
        if [ "$PV_BAD" -eq 0 ]; then
            echo "ctron pkg verify OK: $PV_DIR($PV_N 成员,self_digest sha256:$PV_SUM)"
            exit 0
        fi
        exit 1
        ;;
```

- [ ] **Step 5: 自检六姿态**(正例/篡改/缺件/越界/用法/帮助)

```bash
D=$(mktemp -d)
sh compiler/ctc.sh ast tests/artifact_demo/base/base.ct --ast=seal --astout="$D/a" --astname=mybase >/dev/null
./ctron pkg verify "$D/a"; echo "rc=$?"          # 期望 OK 行 + rc=0
printf 'X' >> "$D/a/impl/base.ast"
./ctron pkg verify "$D/a"; echo "rc=$?"          # 期望 成员摘要不符: impl/base.ast + rc=1
rm "$D/a/SHA256SUMS"; git checkout -- /dev/null 2>/dev/null; sh compiler/ctc.sh ast tests/artifact_demo/base/base.ct --ast=seal --astout="$D/a" --astname=mybase >/dev/null
rm "$D/a/SHA256SUMS"
./ctron pkg verify "$D/a"; echo "rc=$?"          # 期望 缺 SHA256SUMS + rc=1
printf '%s  ../evil\n' "$(shasum -a 256 "$D/a/SHA256SUMS" | cut -d' ' -f1)" >> "$D/a/SHA256SUMS"
./ctron pkg verify "$D/a"; echo "rc=$?"          # 期望 路径越界 + rc=1
./ctron pkg bogus; echo "rc=$?"                  # 期望 rc=2
./ctron pkg --help | head -1                     # 期望 help 文案
rm -rf "$D"
```

Expected: 六姿态 rc 与文案全数命中。

### Task 2: smoke 3r 腿(六断言,复用 3p 腿 `$DG/bh` 已封工件)

**Files:**
- Modify: `compiler/test/smoke.sh`(T50 3p 腿 `match/enum 工件异常` fi 结束后、`# ---------------- 3p) T40/T42 bare 档锚` 注释行前插入)

**Interfaces:**
- Consumes: Task 1 命令面(经 `"$ROOT/ctron"` 调用);3p 腿变量 `DG="$T/dg"` 与已封 `$DG/bh`(诚实 mybase,含 self_digest+SHA256SUMS)。
- Produces: 3r 腿六断言,计数入全量 smoke。

- [ ] **Step 1: 插入 3r 腿**(完整代码):

```sh
echo "== 3r) T50/L1 ctron pkg verify 命令面(工件摘要自洽 fail-closed)=="
if "$ROOT/ctron" pkg verify "$DG/bh" > "$T/q1.out" 2>&1; then
    grep -q "pkg verify OK" "$T/q1.out" && ok "pkg verify 正例:诚实工件自洽过验" || bad "OK 无文案: $(cat "$T/q1.out")"
else
    bad "pkg verify 正例误拒: $(cat "$T/q1.out")"
fi
rm -rf "$DG/vf" && cp -r "$DG/bh" "$DG/vf" && printf 'X' >> "$DG/vf/impl/base.ast"
if "$ROOT/ctron" pkg verify "$DG/vf" > "$T/q2.out" 2>&1; then
    bad "篡改 impl 字节未检出"
else
    grep -q "成员摘要不符: impl/base.ast" "$T/q2.out" && ok "成员腿:篡改 impl 字节点名路径与双摘要" || bad "无成员不符文案: $(cat "$T/q2.out")"
fi
rm -rf "$DG/vs" && cp -r "$DG/bh" "$DG/vs"
awk 'NR==1{c=substr($0,1,1); r=(c=="0")?"1":"0"; sub(/^./,r)} {print}' "$DG/bh/SHA256SUMS" > "$DG/vs/SHA256SUMS"
if "$ROOT/ctron" pkg verify "$DG/vs" > "$T/q3.out" 2>&1; then
    bad "SHA256SUMS 记录被改未检出"
else
    grep -q "成员摘要不符" "$T/q3.out" && ok "成员腿:SHA256SUMS 记录行翻改即不符" || bad "无成员不符文案: $(cat "$T/q3.out")"
fi
rm -rf "$DG/vm" && cp -r "$DG/bh" "$DG/vm" && rm "$DG/vm/SHA256SUMS"
if "$ROOT/ctron" pkg verify "$DG/vm" > "$T/q4.out" 2>&1; then
    bad "缺 SHA256SUMS 未拒"
else
    grep -q "缺 SHA256SUMS" "$T/q4.out" && ok "形态腿:缺 SHA256SUMS fail-closed 拒载" || bad "无缺失文案: $(cat "$T/q4.out")"
fi
rm -rf "$DG/ve" && cp -r "$DG/bh" "$DG/ve"
VEH=$(head -1 "$DG/ve/SHA256SUMS" | cut -d' ' -f1)
printf '%s  ../evil\n' "$VEH" >> "$DG/ve/SHA256SUMS"
if "$ROOT/ctron" pkg verify "$DG/ve" > "$T/q5.out" 2>&1; then
    bad "成员路径越界未拒"
else
    grep -q "路径越界" "$T/q5.out" && ok "越界腿:成员路径含 .. 拒绝(不可信输入面)" || bad "无越界文案: $(cat "$T/q5.out")"
fi
rm -rf "$DG/vd" && cp -r "$DG/bh" "$DG/vd"
awk '/self_digest/{sub(/sha256:./,"sha256:x")} {print}' "$DG/bh/meta.ctcl" > "$DG/vd/meta.ctcl"
if "$ROOT/ctron" pkg verify "$DG/vd" > "$T/q6.out" 2>&1; then
    bad "self_digest 记录被改未检出"
else
    grep -q "self_digest 不符" "$T/q6.out" && ok "self_digest 腿:记录串与 SHA256SUMS 字节重算不符" || bad "无 self_digest 文案: $(cat "$T/q6.out")"
fi
```

(翻改皆保证相异:SHA256SUMS 首字符按是否 "0" 取 "1"/"0";self_digest 首个 hex 字符替换为非 hex 的 "x"。)

- [ ] **Step 2: 单腿跑验**

Run: `sh compiler/test/smoke.sh 2>&1 | sed -n '/== 3r)/,/^== /p' | head -20`
Expected: 3r 腿六行全 `ok`、零 FAIL(3p 腿 `bh` 工件在其前已封,顺序依赖成立)。

### Task 3: 全量门禁 + 单提交落库

- [ ] **Step 1: 全量 smoke**

Run: `sh compiler/test/smoke.sh 2>&1 | tail -3`
Expected: ok 数较 D8-2 基线(166)增 6 = **172 ok**;fail 维持主树在册红(2:conc_parallel 发射红 + Rust 臂 iter 翻绿待清账),零新增。

- [ ] **Step 2: meta_check 无涉确认**(纯 shell 批,零新码)

Run: `python3 tests/meta_check.py`
Expected: 与主树基线一致(dep_mutex_neg E5049 一败在册),无新错。

- [ ] **Step 3: 单提交落库**

```bash
git add ./ctron compiler/test/smoke.sh docs/superpowers/plans/2026-10-06-t50-closed-pkg-l1-pkg-verify.md
git commit -m "feat(pkg): T50 闭源分发——L1 ctron pkg verify 命令面(SHA256SUMS 逐成员重算+self_digest 重算+形态 fail-closed;纯 shell 编排层,编译器零触碰;用户点名触发)"
```

提交体含:L1 落地申报、trace 复放腿归 S3 挂账、基线红归因(2 smoke + 1 meta_check 主树在册)。

### Task 4: 台账回写 + memory

**Files:**
- Modify: `docs/superpowers/plans/2026-09-21-closed-pkg-s0-iface.md`(D8-2 L2 落地节后加 L1 落地节)
- Modify: `docs/superpowers/specs/2026-09-21-closed-pkg-distribution-design.md`(§10 S2 落地注余债行:L1 划销,注 trace 复放腿归 S3)
- Memory: `ctron-closed-pkg-lane.md` 更新余债与落点

- [ ] **Step 1: 泳道账本加 L1 落地节**(D8-2 节后;要点:命令面四段算法/3r 六断言/smoke 172 基线/余债更新为 L3+命名空间+S1b)

- [ ] **Step 2: spec §10 S2 落地注余债行改写**:`余:S1b 缓存接线(…)、L3 in-language sha256 重算(载体重=lib/std/crypto.ct sha256_hex 已证)、跨发布者命名空间(S4;trace 复放腿亦归 S3)`(L1 划销)

- [ ] **Step 3: memory `ctron-closed-pkg-lane.md` 更新**(余债行与坑位如有新坑则补)

## Self-Review

- Spec 覆盖:§7.2 离线路径 `ctron pkg verify`(校验 SHA256SUMS)→ Task 1 命令面;§3.5 形态 fail-closed(名实不符即失败)→ Task 1 ①段;篡改面(D8-2 L1 职责)→ Task 1 ②段 + Task 2 成员/记录双腿;self_digest 循环免定义保持 → ③段只读重算不写。**Gap 如实登记**:§7.2 的"复放轨迹"半句 = S3,不在本批,落地注点名。
- 占位符扫描:无 TBD/TODO;Task 1 Step 5 自检含 `git checkout -- /dev/null` 为无害空操作噪声,执行时省去即可(计划保留原样供对照)。
- 类型一致:文案串五枚(`pkg verify OK`/`成员摘要不符: `/`缺 SHA256SUMS`/`路径越界`/`self_digest 不符`)Task 1 实现与 Task 2 断言逐字对位;变量 PV_ 前缀单臂作用域;3r 标签唯一。
