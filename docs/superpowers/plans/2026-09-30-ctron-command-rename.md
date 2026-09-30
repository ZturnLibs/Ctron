# 工具链命名收口:用户驱动 ctc→ctron、Rust 参考实现 ctron→ctronr 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 命名三分法定型——`ctron` = 正身用户驱动(自举版,即今日 `ctc`)、`ctronc` = C 宿主(不动)、`ctronr` = Rust 参考实现;后缀字母即实现语言,裸名归真身。

**Architecture:** 纯改名收口,零行为变更。三个硬决策已由用户裁决:① 硬切(同 commit 删 ctc/ctc.ps1/ctc.cmd,不留兼容壳,网站/README 同步主推 ctron);② 开发期自举驱动 `compiler/ctc.sh` 名不变(纯内部工具,40+ 处引用不联改);③ 五件套 `ctron-{cc,chk,emit,fmt,doc}` 名不变(它们已带 ctron- 前缀,与新驱动名 `ctron` 构成家族)。

**Tech Stack:** POSIX sh(Makefile/install.sh/release.sh/ctc_smoke)、PowerShell/Batch(Windows 双垫片,Colon.ps1 须保 UTF-8 BOM)、Cargo(Rust bin 名)、GitHub Actions(release.yml)、mkdocs markdown(website/)。

## Global Constraints

- **硬切**:不保留任何 `ctc` 兼容壳;`install.sh` 顺带清除旧装机残留的 `bin/ctc`(这是同职脚本的旧名,不是旧布局目录,直接删,与"旧布局仅提示"先例不冲突)。
- **不改名清单**:`compiler/ctc.sh`(dev seed 驱动)、`ctronc`(C 宿主)、`ctron-{cc,chk,emit,fmt,doc}`(五件套)、`CTRON_*` 环境变量、`ctron:link` 等语言面属性、GitHub 仓库/包名。
- **机刷竞态防卷**:提交一律用 only 模式 `git commit -m "..." -- <paths>`(只取所列路径的工作树内容,不碰索引里对端泳道的暂存件),禁 `git add -A`、禁裸 `git commit`;每次提交后 `git show --stat HEAD` 核对仅含本任务文件,混入他件立即修复。执行前重查 `git status`——当前 eval-repr 泳道在飞件含 `compiler/test/smoke.sh`(staged M),该文件的改动隔离在 Task 6,其余任务文件均不在对端 staged 集。遇 git `index.lock` 冲突等 3 秒重试一次(对端可能在同时提交)。
- **ctron.ps1 编码**:必须保持 UTF-8 with BOM(`EF BB BF`),改后必验 `head -c 3 | od -An -tx1`。
- **macOS BSD sed**:`sed -i ''`(空备份后缀);GNU 的 `\b` 不可用,词边界用 `[[:<:]]` `[[:>:]]`。
- **⚠ 核心替换纪律(哨兵保护法)**:词边界**并不**保护 `ctc.sh`——`.` 是非词字符,`[[:<:]]ctc[[:>:]]` 照样命中 `ctc.sh` 里的 `ctc`,裸词替换会把 dev 驱动打成 `ctron.sh`。凡对可能含 `ctc.sh` 引用的文件做替换,必须三段式:
  `s/ctc\.sh/@@CTCDOTSH@@/g`(护)→ `s/[[:<:]]ctc[[:>:]]/ctron/g`(换,顺带正确处理 `ctc.ps1`/`ctc.cmd`→`ctron.ps1`/`ctron.cmd`)→ `s/@@CTCDOTSH@@/ctc.sh/g`(复)。
  `ctc_smoke` 因 `_` 是词字符不会被裸词规则命中,需要处显式 `s/ctc_smoke/ctron_smoke/g`。**任何文件上禁用无边界 `s/ctc/ctron/g`**。
- **替换判别规则**(全计划通用):根驱动语义的 `ctc` → `ctron`;`ctc.ps1`/`ctc.cmd` → `ctron.ps1`/`ctron.cmd`;凡指 `compiler/ctc.sh`(dev seed 驱动)一律保留;"Ctron"语言名、"ctronc"、五件套名不动。
- **验证基线口径**:ctron_smoke 24/0;fmt parity 300+ 零分歧;Rust 12 个 cargo 测试目标全绿。

---

### Task 0: 前置对齐与基线(无 commit)

**Files:** 无改动,只读核查。

- [ ] **Step 1: git 状态对齐**

```bash
cd /Users/zyj/Zturn/Ctron && git status --short && git log --oneline -3
```

判定:`compiler/test/smoke.sh` 是否仍在 staged/未落库集(对端 eval-repr 泳道)。若是 → Task 6 挂起,待对端落库后单独执行;其余任务不受影响(文件集不相交)。

- [ ] **Step 2: 建立改名前绿基线**

```bash
make -C compiler-c                      # ctronc(parity 需)
[ -x compiler/bin/ctron-cc ] || sh compiler/native.sh   # 五件套(ctron_smoke 需)
sh tests/dist/ctc_smoke.sh              # 期望 24/0
sh tests/fmt/parity.sh                  # 期望 300+ 零分歧
```

记录两组数字作为对照基线;任一红先修基线再动改名。

---

### Task 1: Rust 参考实现 ctron → ctronr

**Files:**
- Modify: `compiler-rust/Cargo.toml`(`[[bin]]` 块,约 L11-12)
- Modify: `compiler-rust/tests/test_suite.rs:2,95`、`compiler-rust/tests/fmt_suite.rs:291`(`CARGO_BIN_EXE_ctron` 宏随 bin 名自动更名)
- Modify: `compiler-rust/examples/perf.rs:231-232`
- Modify: `compiler-rust/README.md`(命令行段)
- Modify: `tools/bench.py:20,32`、`tools/campaign.py:7`
- Modify: `tests/fmt/parity.sh:4,12,15`

**Interfaces:**
- Produces: `compiler-rust/target/release/ctronr` 可执行;parity.sh `RUST` 变量指向同名;后续 Task 均依赖 bin 名 `ctronr` 已生效。
- 不改:Cargo 包名(L2 `name = "ctron"`)与 lib 名(L8)保留——内部引用零扰动,crates.io 发布议题远期再议。

- [ ] **Step 1: Cargo.toml 只改 [[bin]] 名**

```toml
[[bin]]
name = "ctronr"
```

(文件里另有两处 `name = "ctron"`:package 块与 lib 块,**不许动**。)

- [ ] **Step 2: 同步三处代码引用**

`compiler-rust/tests/test_suite.rs:95` 与 `compiler-rust/tests/fmt_suite.rs:291`:

```rust
let out = std::process::Command::new(env!("CARGO_BIN_EXE_ctronr"))
```

`test_suite.rs:2` 注释同步:`CLI 级走 CARGO_BIN_EXE_ctronr`。
`compiler-rust/examples/perf.rs:231-232`:

```rust
let rel_bin = manifest.join("target/release/ctronr");
let dbg_bin = manifest.join("target/debug/ctronr");
```

- [ ] **Step 3: 工具与对拍脚本路径**

```bash
sed -i '' 's#target/release/ctron#target/release/ctronr#; s#target/debug/ctron#target/debug/ctronr#' tools/bench.py tools/campaign.py
sed -i '' 's#compiler-rust/target/release/ctron#compiler-rust/target/release/ctronr#g; s/(参考实现,ctron fmt)/(参考实现,ctronr fmt)/' tests/fmt/parity.sh
```

(`tools/bench.py:20` 文档串、`:32` R 路径;`tools/campaign.py:7` CT 路径;`parity.sh:12` 注释、`:15` RUST=。)

- [ ] **Step 4: compiler-rust/README.md 命令面**

仅替换命令调用形态(语言名 "Ctron" 不许动):

```bash
sed -i '' -E 's/[[:<:]]ctron (lex|parse|check|run|test|fmt|build)[[:>:]]/ctronr \1/g' compiler-rust/README.md
grep -n "target/release/ctron\b" compiler-rust/README.md && sed -i '' 's#target/release/ctron#target/release/ctronr#g' compiler-rust/README.md || true
```

- [ ] **Step 5: 构建与全测试门**

```bash
cargo build --release --manifest-path compiler-rust/Cargo.toml
test -x compiler-rust/target/release/ctronr && echo BIN-OK
cargo test --manifest-path compiler-rust/Cargo.toml    # 12 个测试目标全绿
```

Expected: `BIN-OK`;cargo test 12/12 套件绿(bin 名未同步处会在此暴露为编译错——CARGO_BIN_EXE_ctron 宏找不到旧 bin)。

- [ ] **Step 6: fmt 三宿主对拍**

```bash
sh tests/fmt/parity.sh    # 期望 300+ 零分歧,与 Task 0 基线同数
```

- [ ] **Step 7: Commit(pathspec 限定)**

```bash
git commit -m "refactor(naming): Rust 参考实现 bin 更名 ctron→ctronr——三分法落地第一步(裸名让位正身驱动,后缀字母=实现语言)" -- \
  compiler-rust/Cargo.toml compiler-rust/tests/test_suite.rs compiler-rust/tests/fmt_suite.rs \
  compiler-rust/examples/perf.rs compiler-rust/README.md tools/bench.py tools/campaign.py tests/fmt/parity.sh
git show --stat HEAD    # 核对仅含上列八件;混入对端 staged 件立即修复
```

---

### Task 2: 用户驱动三件 git mv(ctc / ctc.ps1 / ctc.cmd → ctron / ctron.ps1 / ctron.cmd)

**Files:**
- Rename: `ctc` → `ctron`、`ctc.ps1` → `ctron.ps1`、`ctc.cmd` → `ctron.cmd`
- Modify: 三件内文(usage/die 前缀/头注/rc 约定注释)

**Interfaces:**
- Produces: 仓库根可执行 `ctron`(dev 回落逻辑不变:同目录无二进制时回落 `<脚本目录>/compiler/bin/ctron-cc`,五件套名未变所以该路径原样可用)。
- 消费方:Task 3(装机/分发引用新名)、Task 4(ctron_smoke 调用新名)。

- [ ] **Step 1: git mv 三件**

```bash
git mv ctc ctron && git mv ctc.ps1 ctron.ps1 && git mv ctc.cmd ctron.cmd
```

- [ ] **Step 2: sh 版内文替换**

```bash
sed -i '' 's/[[:<:]]ctc[[:>:]]/ctron/g' ctron
sh -n ctron && sh ctron --version
```

Expected: `--help`/usage 文本、`die2()` 前缀 `"ctron: "`、头注、rc 约定注释全部带新名;`sh ctron --version` rc=0。
再验 dev 回落态(编译器 bin 在 compiler/bin 时):

```bash
printf 'fn main() { println("hi") }\n' > /tmp/hello_rename.ct
sh ctron run /tmp/hello_rename.ct     # 期望输出 hi
```

- [ ] **Step 3: PowerShell 版内文替换 + BOM 验证**

```bash
sed -i '' 's/[[:<:]]ctc[[:>:]]/ctron/g' ctron.ps1
head -c 3 ctron.ps1 | od -An -tx1      # 期望 ef bb bf(BOM 必须在)
grep -n "ctron.ps1\|命令面基准" ctron.ps1 | head -3
```

Expected: 首行头注、`Usage` here-string 全部命令面、rc 注释 `2 ctron 环境或用法错误`。macOS 无法执行 PowerShell,本任务只做文本面验证(Windows 面在 release 分发时由 §7.9 conformance 覆盖)。

- [ ] **Step 4: cmd 垫片指路**

`ctron.cmd` 全文应为:

```bat
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ctron.ps1" %*
exit /b %ERRORLEVEL%
```

- [ ] **Step 5: Commit**

(git mv 已暂存改名对,sed 内文编辑后由 only 模式提交一并取工作树内容;禁 `git add -A`。)

```bash
git commit -m "refactor(naming): 用户驱动更名 ctc→ctron(硬切)——正身驱动领裸名,五件套/dev ctc.sh/ctronc 均不动" -- \
  ctc ctc.ps1 ctc.cmd ctron ctron.ps1 ctron.cmd
git show --stat HEAD    # 期望恰三对 rename;混入对端 staged 件立即修复
```

---

### Task 3: 构建/装机/分发包面

**Files:**
- Modify: `Makefile:18`
- Modify: `install.sh:39,43` + 新增残留清理块
- Modify: `tools/release.sh:33,61,62(注释),64`
- Modify: `.github/workflows/release.yml:107`

**Interfaces:**
- Consumes: Task 2 的 `ctron`/`ctron.ps1`/`ctron.cmd` 已存在。
- Produces: 装机布局 `~/.ctron/bin/ctron`(四件套同名不变);release tarball `ctron/bin/ctron`。

- [ ] **Step 1: Makefile 安装行**

L18 改为:

```make
	install bin/ctron-cc bin/ctron-chk bin/ctron-emit bin/ctron-fmt ctron $(DEST)/bin/
```

- [ ] **Step 2: install.sh 三处 + 残留清理**

L39:`"$DIR/bin/ctc" --version` → `"$DIR/bin/ctron" --version`。
L43:`ctc run/check 不需要;ctc build 需要` → `ctron run/check 不需要;ctron build 需要`。
L34(`cp -R "$TMP"/ctron/. "$DIR/"`)之后插入(set -eu 下必须用 if 守卫,`[ -f ] && rm` 链在文件缺失时会以非零杀掉整个脚本):

```sh
# 更名收口(2026-09-30):用户驱动 ctc 更名 ctron;清掉旧装机残留的同职脚本
if [ -f "$DIR/bin/ctc" ]; then
    rm -f "$DIR/bin/ctc"
    echo "install.sh: 已移除旧命令 $DIR/bin/ctc(已更名 ctron)"
fi
```

- [ ] **Step 3: release.sh 三处**

L33:`install -m 755 "$DIR/ctc" "$PKG/bin/ctc"` → `install -m 755 "$DIR/ctron" "$PKG/bin/ctron"`。
L61:`cp "$DIR/ctc" "$SRC/"` → `cp "$DIR/ctron" "$SRC/"`。
L62 注释与 L64:`cp "$DIR/ctc.ps1" "$SRC/"; cp "$DIR/ctc.cmd" "$SRC/"` → `cp "$DIR/ctron.ps1" "$SRC/"; cp "$DIR/ctron.cmd" "$SRC/"`,注释同步 `spec §2.3 源码线布局含 ctron.ps1/ctron.cmd`。
(L49-52 `compiler/ctc.sh emit` 四行 **保留不动**——dev 驱动。)

- [ ] **Step 4: release.yml Windows 资产**

L107:`cp bin/*.exe ctron/bin/; cp ctc.ps1 ctc.cmd ctron/bin/` → `cp bin/*.exe ctron/bin/; cp ctron.ps1 ctron.cmd ctron/bin/`。

- [ ] **Step 5: 装机仿真验证**

```bash
make clean && make
make install PREFIX=/tmp/ctron_rename_test
ls /tmp/ctron_rename_test/bin/            # 期望含 ctron、四件套;无 ctc
/tmp/ctron_rename_test/bin/ctron --version
sh -n install.sh && sh -n tools/release.sh
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/release.yml'))" && echo YAML-OK
rm -rf /tmp/ctron_rename_test
```

- [ ] **Step 6: Commit**

```bash
git commit -m "refactor(naming): 装机/分发包面随驱动更名——Makefile/install.sh/release.sh/release.yml 装 ctron,install.sh 清旧 ctc 残留(if 守卫防 set -e 误杀)" -- \
  Makefile install.sh tools/release.sh .github/workflows/release.yml
git show --stat HEAD    # 核对仅含上列四件
```

---

### Task 4: 冒烟门更名 ctc_smoke → ctron_smoke + ci.sh 标题

**Files:**
- Rename: `tests/dist/ctc_smoke.sh` → `tests/dist/ctron_smoke.sh`(内文全改)
- Modify: `ci.sh:23,25`

**Interfaces:**
- Consumes: Task 2 的根 `ctron`。
- Produces: ci.sh [5/9] 门走 `tests/dist/ctron_smoke.sh`,24/0。

- [ ] **Step 1: git mv + 内文替换**

```bash
git mv tests/dist/ctc_smoke.sh tests/dist/ctron_smoke.sh
sed -i '' -e 's/[[:<:]]CTC[[:>:]]/CTRON/g' -e 's/ctc_smoke/ctron_smoke/g' -e 's/[[:<:]]ctc[[:>:]]/ctron/g' tests/dist/ctron_smoke.sh
```

(变量名 `CTC` 是大写,裸词小写规则不命中,须独立大写规则覆盖 `CTC="$ROOT/ctc"` 及全部 `$CTC` 引用;`ctc_smoke` 覆盖 mktemp 模板与错误前缀;小写裸词覆盖头注断言集与各段命令调用。文件内无 `ctc.sh` 引用,无需哨兵;改完人工过一遍全文确认。)覆盖点:头注(含「ci.sh [5/9]」更正与断言集九条里的命令名)、`CTRON="$ROOT/ctron"` 变量及全部 `$CTRON` 引用、`ctron_smoke:` 错误前缀、`mktemp -d /tmp/ctron_smoke.XXXXXX`、第 1-9 段全部命令调用与提示文案。

- [ ] **Step 2: ci.sh 两处**

L23:`echo "[5/9] ctc 驱动冒烟(...)"` → `echo "[5/9] ctron 驱动冒烟(run/check/build/fmt 契约/help/无 cc 路径)"`。
L25:`sh "$DIR/tests/dist/ctc_smoke.sh"` → `sh "$DIR/tests/dist/ctron_smoke.sh"`。

- [ ] **Step 3: 门验证**

```bash
sh tests/dist/ctron_smoke.sh     # 期望 24/0,与 Task 0 基线同数
```

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(naming): 冒烟门随驱动更名——ctc_smoke→ctron_smoke(24/0 绿),ci.sh [5/9] 标题/路径同步" -- \
  tests/dist/ctron_smoke.sh tests/dist/ctc_smoke.sh ci.sh
git show --stat HEAD    # 期望恰一对 rename + ci.sh
```

---

### Task 5: 文档/网站/示例/活规范面

**Files:**
- Modify: `README.md:13-15`
- Modify: `docs/intro.md:12,55,56`
- Modify: `website/docs/download.md`、`download.zh.md`、`getting-started.md`、`getting-started.zh.md`、`index.md`、`index.zh.md`、`examples.zh.md`
- Modify: `examples/todo_app/run.sh:11`(注释「ctc test 全绿」)
- Modify: `tests/COVERAGE.md:401`(`ctc fmt`→`ctron fmt`;`ctc_smoke 第 9 段`→`ctron_smoke 第 9 段`)
- Modify: `docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md`(42 处中根驱动语义者)

**Interfaces:**
- Produces: 全部用户可见文档主推 `ctron`;spec 与实现一致(该 spec 随 release 装入 share/doc,是分发面契约文书)。

- [ ] **Step 1: 哨兵三段式逐文件替换**

对上列每个文件统一执行(全局约束的哨兵保护法;COVERAGE.md 含大量 `ctc.sh` 引用,网站文件可能也有):

```bash
FILES="README.md docs/intro.md examples/todo_app/run.sh tests/COVERAGE.md \
  website/docs/download.md website/docs/download.zh.md website/docs/getting-started.md website/docs/getting-started.zh.md \
  website/docs/index.md website/docs/index.zh.md website/docs/examples.zh.md"
for f in $FILES; do
  sed -i '' -e 's/ctc\.sh/@@CTCDOTSH@@/g' -e 's/[[:<:]]ctc[[:>:]]/ctron/g' -e 's/@@CTCDOTSH@@/ctc.sh/g' -e 's/ctc_smoke/ctron_smoke/g' "$f"
done
```

(裸词规则顺带把 `ctc.ps1`/`ctc.cmd` 正确换成 `ctron.ps1`/`ctron.cmd`;`ctc_smoke` 规则覆盖 COVERAGE.md:401 的「ctc_smoke 第 9 段」。)
然后 `git diff` 人工过一遍:确认 "Ctron" 语言名、`compiler/ctc.sh`、`ctronc`、五件套名零误伤。

- [ ] **Step 2: 分发 spec 更名记 + 命令面更新**

`2026-09-17-toolchain-distribution-design.md` 文首(标题行之后)加:

```markdown
> **更名记(2026-09-30)**:用户驱动 `ctc` 更名 **`ctron`**(硬切,无兼容壳;三分法:`ctron` 正身驱动 / `ctronc` C 宿主 / `ctronr` Rust 参考实现)。本文所涉命令名随文更新;开发期自举驱动 `compiler/ctc.sh` 名不变。
```

正文按全局约束的哨兵三段式替换(spec 同时含 `ctc.sh` 与 `ctc.ps1`/`ctc.cmd` 引用;裸词规则会正确换出后者,哨兵护住前者):

```bash
sed -i '' -e 's/ctc\.sh/@@CTCDOTSH@@/g' -e 's/[[:<:]]ctc[[:>:]]/ctron/g' -e 's/@@CTCDOTSH@@/ctc.sh/g' -e 's/ctc_smoke/ctron_smoke/g' \
  docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md
```

改完全文复核:`grep -n "ctc" docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md` 期望仅剩 `ctc.sh` 体系。

- [ ] **Step 3: 残留扫描(本任务文件集)**

```bash
grep -rn "[[:<:]]ctc[[:>:]]\|ctc\.ps1\|ctc\.cmd\|ctc_smoke" README.md docs/intro.md website examples/todo_app/run.sh tests/COVERAGE.md \
  docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md
```

Expected: 仅 `ctc.sh`(dev 驱动)引用命中;零裸 `ctc`、零 `ctc.ps1`/`ctc.cmd`、零 `ctc_smoke`。

- [ ] **Step 4: Commit**

```bash
git commit -m "docs(naming): 用户面文档/网站/spec 随驱动更名——上手页主推 ctron;分发 spec 加更名记(三分法);ctc.sh 体系保留" -- \
  README.md docs/intro.md website examples/todo_app/run.sh tests/COVERAGE.md \
  docs/superpowers/specs/2026-09-17-toolchain-distribution-design.md
git show --stat HEAD    # 核对仅含上列各件(website 为目录下实际改动文件)
```

---

### Task 6: smoke.sh RUSTBIN 行(前置:对端 eval-repr 泳道已落库)

**Files:**
- Modify: `compiler/test/smoke.sh:383`

**Interfaces:**
- Consumes: Task 1 的 `target/release/ctronr`。
- 前置门:Task 0 判定该文件无对端未落库改动(git status 干净或改动已落库)才可执行;否则整个 Task 挂起,绝不与对端 staged 改动同 commit。

- [ ] **Step 1: 改 RUSTBIN**

L383:

```sh
RUSTBIN="$ROOT/compiler-rust/target/release/ctronr"
```

(全文件仅此一处 rust bin 引用,改后 `grep -n "target/release/ctron" compiler/test/smoke.sh` 期望空。)

- [ ] **Step 2: 全量冒烟**

```bash
sh compiler/test/smoke.sh --full     # 全量(含 rust 臂);时间紧时至少跑至 rust 臂相关段全绿
```

- [ ] **Step 3: Commit(pathspec 单文件)**

```bash
git commit -m "refactor(naming): smoke.sh rust 臂路径随 ctronr 更名(对端泳道落库后补片)" -- compiler/test/smoke.sh
git show --stat HEAD    # 核对仅此一件
```

---

### Task 7: 全量门 + 残留终扫 + 记忆收口

**Files:** 无新改动(验证与收口)。

- [ ] **Step 1: ci.sh 全量九门**

```bash
bash ci.sh
```

Expected: 九门全绿(与基线同口径);时间预算紧张时的最小等效集 = `sh tests/dist/ctron_smoke.sh`(24/0)+ `sh compiler/test/smoke.sh --full` + `sh tests/fmt/parity.sh`(300+)。

- [ ] **Step 2: 全仓残留终扫**

```bash
grep -rn "[[:<:]]ctc[[:>:]]\|ctc\.ps1\|ctc\.cmd\|ctc_smoke" . \
  --exclude-dir=.git --exclude-dir=.worktrees --exclude-dir=target --exclude-dir=node_modules --exclude-dir=dist \
  --exclude-dir=plans --include="*.sh" --include="*.md" --include="*.ps1" --include="*.cmd" --include="Makefile" --include="*.yml" --include="*.py"
```

Expected: 命中全部属于 `compiler/ctc.sh` 体系(dev 驱动本体 + 其引用方:smoke.sh/ladder.sh/native.sh/bench.sh/BOOTSTRAP.md/README(compiler)/probe.yml/release.sh 残留注释/docs 历史分析档)与 `docs/superpowers/plans/` 历史计划档(不改)。零裸 `ctc` 用户驱动引用、零 `ctc.ps1`/`ctc.cmd`、零 `ctc_smoke`。

- [ ] **Step 3: 记忆收口**

更新 auto-memory:`ctron-server-lane` 之外的命名口径(三分法 ctron/ctronc/ctronr、ctc.sh 保留裁决、ctron_smoke 新名)写入 `ctron-compile-driver-gotchas` 或新建 `ctron-naming-convention`。

---

## Self-Review 记录

- **覆盖面**:ctc 裸词(README/intro/COVERAGE/todo_app/spec/website 7 件/ci.sh/ctc_smoke/install/Makefile/release.sh/release.yml)与 ctc.ps1/ctc.cmd(release.sh/release.yml/ Task 2)各有任务;Rust bin(Cargo/两测试宏/perf/bench.py/campaign.py/parity.sh/smoke.sh:383/README-rust)全覆盖;spec §2.3 Windows 垫片条款随 Task 5 更新。
- **已知不改面(裁决内)**:`compiler/ctc.sh` 体系、五件套、ctronc、CTRON_* env、`docs/superpowers/plans/` 历史档、`.worktrees/` 旧枝副本。
- **类型一致性**:bin 名统一 `ctronr`(`CARGO_BIN_EXE_ctronr` 宏名随 bin 名,cargo 自动生成);驱动名统一 `ctron`;冒烟门文件名统一 `tests/dist/ctron_smoke.sh`。
- **占位符自检**:Task 5 Step 2 的 sed 占位行仅为警示示例,实际执行两条显式规则,已标注;其余步骤均为精确命令/精确内容。
