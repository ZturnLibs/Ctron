# 库根布局与包分区(lib/ + pkgs/)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 spec `docs/superpowers/specs/2026-09-30-libroot-layout-design.md`:仓库收拢为 `lib/`(std+五域)+`pkgs/`(四包),装机 `~/.ctron/{bin,lib,pkgs}`,loader 增 site root 探测与 W8902 带出路诊断。

**Architecture:** loader 域根规则(libroot=std 根父目录)原文不动,只在解析链 libroot 之后插 site root 探测(`CTRON_SITEPATH` 覆盖或 `dirname(libroot)/pkgs` 同级推导);物理迁移纯 git mv+脚本 sed;分发面三脚本改路径。解析层静默契约不破——包 miss 只收集,W8902 由驱动层在 sem 失败时附加打印。

**Tech Stack:** Ctron 自举编译器(parse_pkg.ct/driver_*.ct)、smoke.sh/suite.py 门禁、sh 脚本面。

## Global Constraints(摘自 spec,全任务隐含)

- 解析链 = 目录相对 → libroot → **site root** → ctart 保底;保底位不动;
- 未命中保持静默(3bcc61e 契约);`CTRON_STDPATH` 名与语义不动;③回落 `dir/../std` 一字不动;
- ②探测串三处同步改:`../lib/ctron/std`→`../lib/std`(parse_pkg.ct L278/L294 + driver_doc.ct L645);
- inline 实现,loader 新逻辑**不新增顶层 fn 除 wrapper**(decl 锁核对);
- 全程 pathspec 限定提交;**不碰在制文件**(当前 M:driver_emit/eval_call/sem_calls/sem_type/trans_expr)——driver_emit.ct 的 W8902 seam 因此走 wrapper 避让,登记后补;
- 分区:lib/={std,net,http,tls,db,ffi}+门面 net.ct/tls.ct/ffi.ct;pkgs/={web,gui,s3,pkg}+gui.ct;vendor/ 不动;
- 硬切文化:旧路径不命中响亮,不留兼容别名。

---

### Task 1: W1-a loader ②探测串去重(三处)

**Files:**
- Modify: `compiler/src/parse_pkg.ct:278`、`compiler/src/parse_pkg.ct:294`
- Modify: `compiler/src/driver_doc.ct:645`

**Interfaces:**
- Produces: ②探测统一指向 `<exe_dir>/../lib/std`(装机新布局);repo 内②本就不命中,零行为变化。

- [ ] **Step 1: 改三处字符串与注释**

parse_pkg.ct L270 注释、L278、L286 注释、L294:
```text
../lib/ctron/std  →  ../lib/std
```
(共 2 处代码串 + 2 处注释;driver_doc.ct L633 注释 + L645 同理。)

- [ ] **Step 2: 快验——重编+冒烟快面**

Run: `sh compiler/build.sh && sh compiler/native.sh && sh compiler/test/smoke.sh`
Expected: 冒烟全 ok(repo ②不命中,纯字符串迁移无行为面)。

- [ ] **Step 3: Commit(W1 与后续小步合并提交亦可,见 Task 4)**

### Task 2: W1-b site root 探测 + wrapper 线程

**Files:**
- Modify: `compiler/src/parse_pkg.ct:302-309`(pkg_load_use 签名)、`:425-433` 之间(插入点=L425 `}` 后、L426 ctart 注释前)

**Interfaces:**
- Produces: `fn pkg_load_use_m(file: List[Str], dir: Str, stack: List[Str], diags: List[Str], misses: List[Str]) -> List[Str]`(新 wrapper,+1 顶层 decl);原 `pkg_load_use` 签名不动(在制 driver_emit.ct:167 零扰动);`pkg_load_use_done` 增尾参 `misses: List[Str]`(内部传递,非顶层 decl)。
- site 取值:①`CTRON_SITEPATH` ②`dirname(libroot)/pkgs`。

- [ ] **Step 1: pkg_load_use_done 增 misses 尾参**(L340 签名 + pkg_load_use 内部调用点 L307 传 `List[Str]()` 占位)

- [ ] **Step 2: 插 site root 探测块**(L425 后、ctart 前;镜像 L400-424 域根块风格,变量名加 2 后缀防作用域歧义)

```crofton
                // site root(2026-09-30 库根布局):①CTRON_SITEPATH ②库根同级
                // ../pkgs(dirname(libroot)/pkgs)下 <首段>[/<次段>…].ct 命中则用之
                // ——libroot 探测后、ctart 保底前;未命中保持静默(收集入 misses)。
                if !fs_exists(mpath) {
                    var spath = env_get("CTRON_SITEPATH")
                    if spath.len == 0 {
                        var sroot2 = pkg_std_root(dir)
                        if sroot2.len > 0 {
                            var cut2: I32 = 0 - 1
                            var ci2: I32 = sroot2.len - 1
                            while ci2 >= 0 {
                                if byte_at(sroot2, ci2) == 47 {
                                    cut2 = ci2
                                    break
                                }
                                ci2 -= 1
                            }
                            if cut2 > 0 {
                                var lroot2 = byte_slice(sroot2, 0, cut2)
                                var cut3: I32 = 0 - 1
                                var ci3: I32 = lroot2.len - 1
                                while ci3 >= 0 {
                                    if byte_at(lroot2, ci3) == 47 {
                                        cut3 = ci3
                                        break
                                    }
                                    ci3 -= 1
                                }
                                if cut3 > 0 {
                                    spath = byte_slice(lroot2, 0, cut3) + "/pkgs"
                                }
                            }
                        }
                    }
                    if spath.len > 0 {
                        var ppath = spath
                        var j2d: I32 = 1
                        while j2d < segs.len {
                            ppath = ppath + "/" + segs[j2d]
                            j2d += 1
                        }
                        ppath = ppath + ".ct"
                        if fs_exists(ppath) {
                            mpath = ppath
                        }
                    }
                }
```

- [ ] **Step 3: ctart 块后收集 miss**(L433 `}` 之后、L435 注释前)

```crofton
                if !fs_exists(mpath) && segs.len >= 3 {
                    misses.push(segs[1])
                }
```
(单段 use=本地兄弟文件,缺文件不属包 miss,维持旧口径。)

- [ ] **Step 4: wrapper fn**(pkg_load_use 之后新增)

```crofton
// pkg_load_use_m —— pkg_load_use 带 miss 收集(W8902 带出路半面;独立 fn 避碰
// 在制 driver_emit.ct 调用点;emit 驱动 seam 待其落库后一行切换,登记 2026-09-30)
fn pkg_load_use_m(file: List[Str], dir: Str, stack: List[Str], diags: List[Str], misses: List[Str]) -> List[Str] {
    return pkg_load_use_done(gui_ds_postmerge_pre(file), dir, stack, List[Str](), diags, true, misses)
}
```
注意:实现期核对 gui_ds_pre/postmerge 的实际包装序——pkg_load_use 体内是 `gui_ds_postmerge(pkg_load_use_done(gui_ds_premerge(file, unit), dir, stack, List[Str](), diags, true), unit, diags)`,wrapper 应完整镜像此三段(含 unit 线),签名照抄 pkg_load_use 加 misses。

- [ ] **Step 5: 重编+快面**

Run: `sh compiler/build.sh && sh compiler/native.sh && sh compiler/test/smoke.sh`
Expected: 全 ok(Repo 内 site 推导位=仓库父/pkgs 不存在→miss→静默,既有测试零扰动)。

### Task 3: W1-c W8902 驱动层 seam(run/check/doc 三处)

**Files:**
- Modify: `compiler/src/driver_run.ct:67` 一带、`compiler/src/driver_check.ct:283` 一带、`compiler/src/driver_doc.ct:718` 一带
- 不碰: `compiler/src/driver_emit.ct`(在制;登记后补)

**Interfaces:**
- Consumes: `pkg_load_use_m`(Task 2)。

- [ ] **Step 1: 三驱动换调用+sem 失败 seam**

各驱动同形(以 driver_run 为例,L67 起与 sem_walk2 失败分支):
```crofton
            var lmisses = List[Str]()
            file = pkg_load_use_m(file, pkg_dir_of(epath), lstack, ldiags, lmisses)
            ...ldiags 打印段不动...
            var sems = sem_walk2(file, "ANCHORPROFILE")
            if !seq2(sems, "") {
                print(strip_at(sems))
                var mi: I32 = 0
                while mi < lmisses.len {
                    println("W8902: 包 " + lmisses[mi] + " 未命中解析链(项目目录→lib→pkgs→deps);独立发布包试: ctron pkg add " + lmisses[mi])
                    mi += 1
                }
                return 1
            }
```
check/doc 驱动的 sem 失败分支以 `grep -n 'sem_walk2' compiler/src/driver_check.ct compiler/src/driver_doc.ct` 定位,同形插入。

- [ ] **Step 2: 重编+快面**(同 Task 2 Step 5;正例由 Task 4 冒烟节覆盖)

### Task 4: W1-d 冒烟 3m 节 + 门禁 + W1 落库

**Files:**
- Modify: `compiler/test/smoke.sh`(3l S2a 节后新增 3m 节)

- [ ] **Step 1: 写 3m 节**(str.ct 源做双布局兼容,W2 后不再触碰)

```sh
echo "== 3m) site root 解析(库根布局 2026-09-30:同级推导/SITEPATH 覆盖/优先级/W8902)=="
SR="$T/site"; SR2="$T/site_neg"
mkdir -p "$SR/lib/std" "$SR/pkgs/zsite" "$SR2/lib/std" "$SR/alt"
SRC_STR="$COMP/../std/str.ct"; [ -f "$SRC_STR" ] || SRC_STR="$COMP/../lib/std/str.ct"
cp "$SRC_STR" "$SR/lib/std/str.ct"; cp "$SRC_STR" "$SR2/lib/std/str.ct"
cat > "$SR/pkgs/zsite/core.ct" <<'EOF'
// zsite.core —— site root 解析正例夹具
pub fn hi() -> Str { return "site-ok" }
EOF
cat > "$SR/alt/core.ct" <<'EOF'
pub fn hi() -> Str { return "alt-ok" }
EOF
cat > "$T/sitemain.ct" <<'EOF'
use zsite.core.{hi}
fn main() { println(hi()) }
EOF
if CTRON_STDPATH="$SR/lib/std" "$COMP/bin/ctron-cc" run "$T/sitemain.ct" > "$T/s1.out" 2>&1 && grep -q "site-ok" "$T/s1.out"; then
    ok "site root 同级推导命中(dirname(libroot)/pkgs)"
else
    bad "site root 未命中: $(tail -2 "$T/s1.out")"
fi
if CTRON_STDPATH="$SR/lib/std" CTRON_SITEPATH="$SR/alt" "$COMP/bin/ctron-cc" run "$T/sitemain.ct" > "$T/s2.out" 2>&1 && grep -q "alt-ok" "$T/s2.out"; then
    ok "CTRON_SITEPATH 覆盖生效(alt 优先于推导)"
else
    bad "CTRON_SITEPATH 覆盖未生效: $(tail -2 "$T/s2.out")"
fi
cat > "$T/sitemain_neg.ct" <<'EOF'
use zsite.core.{hi}
fn main() { println(hi()) }
EOF
if CTRON_STDPATH="$SR2/lib/std" "$COMP/bin/ctron-cc" run "$T/sitemain_neg.ct" > "$T/s3.out" 2>&1; then
    bad "包缺席应失败却 rc=0"
else
    grep -q "E2020" "$T/s3.out" && ok "包缺席 E2020 兜底" || bad "E2020 未兜底: $(head -2 "$T/s3.out")"
    grep -q "W8902" "$T/s3.out" && grep -q "ctron pkg add zsite" "$T/s3.out" && ok "W8902 带出路(含安装指引)" || bad "W8902 缺失: $(tail -3 "$T/s3.out")"
fi
```
(目录相对优先负例由 3l realdep/既有口径覆盖——目录相对先于 libroot/site 是代码序保证。)

- [ ] **Step 2: 跑门禁**

Run: `sh compiler/build.sh && sh compiler/native.sh && sh compiler/test/smoke.sh && python3 compiler/test/suite.py && sh tests/dist/ctc_smoke.sh`
Expected: 全绿(含 3m 四 ok)。decl 锁若计 parse_pkg 顶层 fn,wrapper +1 → 按锁机制修正基线并在提交信息登记。

- [ ] **Step 3: Commit(pathspec)**

```bash
git add compiler/src/parse_pkg.ct compiler/src/driver_doc.ct compiler/src/driver_run.ct compiler/src/driver_check.ct compiler/test/smoke.sh
git commit -m "feat(loader): 库根布局 W1——②串去重+site root 探测(SITEPATH/同级推导)+W8902 带出路(run/check/doc seam;emit 在制避让登记)+冒烟 3m 四例" -- compiler/src/parse_pkg.ct compiler/src/driver_doc.ct compiler/src/driver_run.ct compiler/src/driver_check.ct compiler/test/smoke.sh
```

### Task 5: W2-a 前置对齐 + git mv 全清单

- [ ] **Step 1: 前置对齐(硬前置)**

Run: `git status --porcelain`
Expected: compiler/src 在制五件(driver_emit/eval_call/sem_calls/sem_type/trans_expr)已落库或归属明确;gui.ct 无未落库改动。未对齐则停,上报,不代提交他人改动。

- [ ] **Step 2: git mv**

```bash
mkdir lib pkgs
git mv std lib/std
git mv net http tls db ffi 顺序逐个: git mv net lib/net; git mv http lib/http; git mv tls lib/tls; git mv db lib/db; git mv ffi lib/ffi
git mv net.ct lib/net.ct; git mv tls.ct lib/tls.ct; git mv ffi.ct lib/ffi.ct
git mv web pkgs/web; git mv gui pkgs/gui; git mv s3 pkgs/s3; git mv pkg pkgs/pkg
git mv gui.ct pkgs/gui.ct
```

- [ ] **Step 3: 全仓路径引用清点**

Run: `grep -rln 'CTRON_STDPATH' tests examples tools ci.sh | wc -l`(基线 82)与 `grep -rn '\$ROOT/std\|\$ROOT/\(net\|http\|tls\|db\|ffi\|web\|gui\|s3\|pkg\)\b' tests examples tools compiler/test selfhosted --include='*.sh' | grep -v '/lib/\|/pkgs/' | wc -l`
产出清单作 Step 4 靶。

### Task 6: W2-b 脚本 sed + 符号链接 + 种副 + smoke 路径

- [ ] **Step 1: STDPATH 面 sed**

```bash
grep -rl 'CTRON_STDPATH' tests examples tools ci.sh | xargs sed -i '' 's|"$ROOT/std"|"$ROOT/lib/std"|g'
```

- [ ] **Step 2: 域路径直引 sed**(以 Step 清点为准;perl 词界防误伤)

```bash
grep -rl '\$ROOT/\(std\|net\|http\|tls\|db\|ffi\|web\|gui\|s3\|pkg\)' tests examples tools compiler/test selfhosted --include='*.sh' --include='*.py' | \
  xargs perl -pi -e 's{\$ROOT/(std|net|http|tls|db|ffi|web|gui|s3|pkg)(?![\w/])}{\$ROOT/lib/$1}g unless m{/lib/|/pkgs/}'
```
注意 pkg→pkgs/pkg 的特例:`$ROOT/pkg` 应指 `pkgs/pkg` 非 `lib/pkg`——先跑 `grep -rn '\$ROOT/pkg' …` 逐处人工定靶(预期仅 registry 相关脚本)。

- [ ] **Step 3: 符号链接重指**

```bash
find tests compiler/test -type l | while read l; do t=$(readlink "$l"); \
  case "$t" in *../../net/*|*../../http/*|*../../tls/*|*../../db/*|*../../ffi/*|*../../gui/*|*../../web/*|*../../s3/*|*../../pkg/*) \
  ln -sf "$(echo "$t" | sed 's|\.\./\.\./\.\./\.\./\(net\|http\|tls\|db\|ffi\|gui\|web\|s3\|pkg\)/|../../../../../lib/\1/|; s|\.\./\.\./\.\./\.\./pkg/|../../../../../pkgs/pkg/|')" "$l";; esac; done
```
(以实际 readlink 形态为准——当前实证 `../../../../net/c_src/…`;gui/web/s3/pkg 链接若指 pkg 域走 pkgs 分支;跑后逐链 readlink 抽验。)

- [ ] **Step 4: smoke.sh std/ 面与种副漂移源**

smoke.sh L235/237 漂移循环源 `std/*.ct` → `lib/std/*.ct`;`grep -n '"std/\|/std/' compiler/test/smoke.sh` 逐处核改(cwd 形 `std/` 若指仓库根同改 `lib/std/`);种副目录 `compiler/test/stdpkg/` 结构不动。

- [ ] **Step 5: 快门禁**

Run: `sh compiler/build.sh && sh compiler/native.sh && sh compiler/test/smoke.sh && python3 compiler/test/suite.py && sh tests/dist/ctc_smoke.sh && sh tests/net/run.sh && sh tests/web/run.sh 2>/dev/null || true`
Expected: 全绿;tests/web 若存在 run.sh 必绿,tests/net 全绿。红则回查 sed 误伤。

### Task 7: W2-c 清噪 + W2 落库

- [ ] **Step 1: 清噪**

```bash
rm -f imefix-151438.bin ul_out.log ul_err.log
printf 'imefix*.bin\nul_*.log\n.DS_Store\nexamples/*/src/main\nexamples/*/src/main.c\n' >> .gitignore
git add .gitignore
```

- [ ] **Step 2: Commit(pathspec 全迁移面)**

```bash
git add -A lib pkgs && git add tests examples tools ci.sh compiler/test selfhosted .gitignore
git commit -m "feat(repo)!: 库根布局 W2 物理迁移——std+五域入 lib/、web/gui/s3/pkg 入 pkgs/+门面归位;82 STDPATH 脚本与域路径直引/夹具链接/种副漂移源同波切换;清噪三件(spec 2026-09-30;硬切:旧根路径不命中即响亮)"
```
(提交前 `git status` 核 staged 无在制他人 hunks。)

### Task 8: W3-a 分发三脚本

**Files:**
- Modify: `tools/release.sh:14,34`、`install.sh:34-37`、`Makefile:5,17-19`

- [ ] **Step 1: release.sh**——`mkdir … $PKG/lib/ctron …` → `$PKG/lib`;`cp -R "$DIR/std/." "$PKG/lib/ctron/std/"` → `cp -R "$DIR/lib/." "$PKG/lib/"`;增 vendor 随发:`mkdir -p "$PKG/vendor" && cp -R "$DIR/vendor/deflate" "$DIR/vendor/tls" "$PKG/vendor/"`;src 件段 `cp -R "$DIR/std" "$SRC/std"` → `cp -R "$DIR/lib" "$SRC/lib"`。

- [ ] **Step 2: install.sh**——`cp -R "$TMP"/ctron/. "$DIR/ctron/"` → `"$DIR/"`;三行提示同步(`$DIR/bin`);`--version` 调用改 `"$DIR/bin/ctc"`;增旧布局检测:
```sh
[ -d "$DIR/ctron" ] && echo "提示: 检测到旧布局 $DIR/ctron(2026-09-30 前),确认新版可用后可自行删除" >&2
```

- [ ] **Step 3: Makefile**——`DEST = $(PREFIX)/ctron` → `DEST = $(PREFIX)`;`install -d … $(DEST)/lib/ctron/std …` → `$(DEST)/lib …`;`cp -R std/. $(DEST)/lib/ctron/std/` → `cp -R lib/. $(DEST)/lib/`。

### Task 9: W3-b ctc_smoke 扩 + 宪章 v2.2 + 文档面

- [ ] **Step 1: ctc_smoke.sh 扩三件**(装机器到 `CTRON_INSTALL_DIR=$T/inst` 跑 install.sh;例:hello 用 `use std.str.*` 与 `use net.bind.*` 头部探针 + site 夹具 `$T/inst/pkgs` 放 zsite 复用 3m 形态;断言输出与 PATH 提示形态)。

- [ ] **Step 2: 宪章 v2.2**——`lib/std/README.md`:物理条款改写(域包表「物理位置=仓库根与 std/ 平级」→「lib/ 与 pkgs/ 分区」;安装布局 `lib/ctron/<域>` → `~/.ctron/{lib,pkgs}`;拆分条件①补「gui 已物理预落位 pkgs/」);种副 README 镜像同步(smoke 漂移断言含 README 则一并)。

- [ ] **Step 3: 规范与文档站**——`grep -rln 'lib/ctron\|std 与.*平级' docs website | 逐处核改`;`tools/sync_site_spec.sh` 跑一遍。

- [ ] **Step 4: Commit(W3 pathspec)**

### Task 10: W4 全量验收

- [ ] **Step 1:** `sh ci.sh`(九门)——Expected: 全绿(GUI 门按平台口径)。
- [ ] **Step 2:** 关键腿:`sh tests/net/run.sh && sh tests/http/run.sh 2>/dev/null; sh tests/web/run.sh; sh tests/gui/run.sh`(headless 阶梯)。
- [ ] **Step 3:** 真装机烟测:`CTRON_INSTALL_DIR=$TMP/ctroninst sh install.sh`(或本地 tarball 经 release.sh)→ `$TMP/ctroninst/bin/ctc run` hello(use std.x)+ use net.x + site zsite 三件(ctc_smoke 已覆盖则引用其绿)。
- [ ] **Step 4:** 收尾提交 + spec 状态行更新(「spec 复审中」→「已实施(本 plan)」)。

## Self-Review 结论

- 覆盖:spec §1(②三处/site/W8902/静默/inline)→ T1-T4;§2(git mv/82 脚本/夹具/种副/清噪/前置对齐)→ T5-T7;§3(release/install/Makefile/ctc_smoke/宪章/文档站)→ T8-T9;§7 波次验收 → T10。无缺口。
- 类型一致:pkg_load_use_m 签名在 T2 定义、T3 消费一致;misses 线程经 done 尾参。
- 已知让步(登记非缺口):driver_emit seam 避让在制(提交信息+代码注释双登记);`$ROOT/pkg` 特例人工定靶。
