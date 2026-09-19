# Ctron GitHub Pages 主页与文档系统 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按规格(docs/superpowers/specs/2026-09-19-website-docs-design.md)建成 MkDocs Material 双语站点五件套 + spec 同步层 + std 参考 + Pages 部署 workflow。

**Architecture:** `website/` 为 mkdocs 项目根(与语言代码分离);spec 经 `tools/sync_site_spec.sh` 单向同步进站(CI 漂移检查);std 参考由 `tools/std_doc.py` 从 `std/*.ct` 扫 `pub fn` 半自动生成;`.github/workflows/pages.yml` push main 时 strict 构建 + deploy-pages;en 树完整规则 = 无后缀文件为 en(待翻页中文占位 + front matter 注记),中文加 `.zh` 后缀。

**Tech Stack:** Python 3 + mkdocs-material + mkdocs-static-i18n(Actions 内装,本地 venv 可选);GitHub Actions(pages)。

## Global Constraints

- 仓库 slug 统一写 `Zturn/Ctron`(建仓时若实际 user/org 不同,`grep -rl 'Zturn/Ctron' website/ .github/workflows/pages.yml` 一次性替换)。
- 文件形态铁律:en(默认)无后缀,中文 `.zh` 后缀;**每个页面必须有无后缀文件**(en 树完整,strict 构建无死链);待翻页的无后缀文件以中文占位 + front matter `<!-- 英文待翻:中文占位 -->` 注记。
- 站点内容口径与分发线一致:`ctc run`/`check` 零依赖;`ctc build` 需本机 cc;Windows 需 mingw-w64;版本 v0.0.1。
- 构建质量门:`mkdocs build --strict`(警告即错)。
- 仓库当前无远端——本计划全部本地可交付;deploy 仅在远端就位后真实发生。
- 并行任务线可能占用工作树:各任务 `git add` 只指名文件。

---

### Task 1: website 骨架与本地构建

**Files:**
- Create: `website/mkdocs.yml`
- Create: `website/docs/index.md`、`website/docs/index.zh.md`(占位,Task 4 填内容)
- Create: `website/requirements.txt`
- Create: `website/.gitignore`(忽略 `site/` 与 `.venv/`)

**Interfaces:**
- Produces: `mkdocs build --strict` 在 `website/` 绿;导航骨架(五件套占位)供 Task 2-4 填充。

- [ ] **Step 1: venv 与依赖**

```bash
python3 -m venv website/.venv
website/.venv/bin/pip install mkdocs-material mkdocs-static-i18n
```

(网络不可用时:跳过本地构建,各任务验证改为 `python3 -c "import yaml;yaml.safe_load(open('website/mkdocs.yml'))"` + 结构断言,真实构建归 CI;如实记入报告。)

- [ ] **Step 2: mkdocs.yml**

```yaml
site_name: Ctron
site_url: https://zturn.github.io/Ctron/
repo_url: https://github.com/Zturn/Ctron
repo_name: Zturn/Ctron
theme:
  name: material
  language: en
  palette:
    - media: "(prefers-color-scheme: light)"
      scheme: default
      toggle: {icon: material/weather-night, name: 暗色}
    - media: "(prefers-color-scheme: dark)"
      scheme: slate
      toggle: {icon: material/weather-sunny, name: 亮色}
  features:
    - content.code.copy
    - navigation.sections
plugins:
  - search
  - i18n:
      docs_structure: suffix
      default_language: en
      languages:
        - locale: en
          default: true
          name: English
          build: true
        - locale: zh
          name: 中文
          build: true
nav:
  - Home: index.md
  - Getting Started: getting-started.md
  - Language Spec: spec/README.md
  - Std Reference: std/README.md
  - Examples: examples.md
  - Download: download.md
```

- [ ] **Step 3: 首页占位两件**

`website/docs/index.md`:

```markdown
<!-- 英文待翻:中文占位 -->
# Ctron

一个自举的系统编程语言:编译器用 Ctron 自己写成,发射等价 C,由平台 C 编译器出机器码。

```bash
curl -fsSL https://github.com/Zturn/Ctron/releases/latest/download/install.sh | sh
```

(内容页 Task 4 填充;本占位保证 en 树完整。)
```

`website/docs/index.zh.md`:

```markdown
# Ctron

一个自举的系统编程语言:编译器用 Ctron 自己写成,发射等价 C,由平台 C 编译器出机器码。

```bash
curl -fsSL https://github.com/Zturn/Ctron/releases/latest/download/install.sh | sh
```
```

- [ ] **Step 4: 其余四件占位(getting-started/examples/download 的 .md 与 .zh.md,各 3 行标题占位;spec/README.md 与 std/README.md 为 Task 2/3 产物,nav 暂指向不存在的文件会红——先建两行占位 `website/docs/spec/README.md`、`website/docs/std/README.md`)**

- [ ] **Step 5: 严格构建**

Run: `cd website && .venv/bin/mkdocs build --strict`
Expected: 零警告退出 0。

- [ ] **Step 6: Commit**

```bash
git add website/mkdocs.yml website/requirements.txt website/.gitignore website/docs/
git commit -m "feat(site): website 骨架——Material+i18n 插件/导航五件套/strict 构建绿"
```

### Task 2: spec 同步脚本与十章入站

**Files:**
- Create: `tools/sync_site_spec.sh`(支持 `--check`)
- Create: `website/docs/spec/*.md`(十章 + README,由脚本生成)

**Interfaces:**
- Produces: `tools/sync_site_spec.sh` 无参=同步(拷贝+front matter 注入),`--check`=重同步到临时目录 diff 已提交件,漂移非零退出;Task 5 workflow 消费 `--check`。

- [ ] **Step 1: 脚本**

```sh
#!/bin/sh
# sync_site_spec.sh —— docs/spec → website/docs/spec 单向同步(规范是活的,防 fork 漂移)
#   无参:同步(覆盖);--check:重同步到临时目录与已提交件 diff,漂移即非零退出
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$DIR/docs/spec"
DST="$DIR/website/docs/spec"
FM='<!-- 站点同步件:源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->'

if [ "${1:-}" = "--check" ]; then
    TMP=$(mktemp -d /tmp/sitespec.XXXXXX) && trap 'rm -rf "$TMP"' EXIT
    for F in "$SRC"/*.md; do
        { printf '%s\n' "$FM"; cat "$F"; } > "$TMP/$(basename "$F")"
    done
    diff -r "$DST" "$TMP"
    echo "sync_site_spec: 无漂移 ✓"
    exit 0
fi

mkdir -p "$DST"
for F in "$SRC"/*.md; do
    { printf '%s\n' "$FM"; cat "$F"; } > "$DST/$(basename "$F")"
done
echo "sync_site_spec: 已同步 $(ls "$DST" | wc -l | tr -d ' ') 件 → website/docs/spec/"
```

- [ ] **Step 2: 同步 + 提交产物**

```bash
chmod +x tools/sync_site_spec.sh && sh tools/sync_site_spec.sh
sh tools/sync_site_spec.sh --check
```
Expected: 同步 11 件(十章+README);--check 无漂移。

- [ ] **Step 3: strict 构建仍绿**

Run: `cd website && .venv/bin/mkdocs build --strict`
Expected: 退出 0(十一个新页进入 Language Spec 节;spec 内部相对链接若 strict 报断链,修法=同步脚本不改、以 mkdocs `not_in_nav`/排除或修源相对链接为站内绝对路径,选择最小者并记录)。

- [ ] **Step 4: Commit**

```bash
git add tools/sync_site_spec.sh website/docs/spec/
git commit -m "feat(site): spec 十章入站——tools/sync_site_spec.sh 单向同步+--check 漂移门"
```

### Task 3: std 参考生成器与模块页

**Files:**
- Create: `tools/std_doc.py`
- Create: `website/docs/std/*.md`(README + 每模块一页)

**Interfaces:**
- Produces: `tools/std_doc.py` 读 `std/*.ct`(排除 gui 子目录的 bind 内部件可保留),按 `pub fn` 正则生成 `website/docs/std/<module>.md`(签名表 + 待补说明占位)与索引 `README.md`;签名正则:`^pub fn (\w+)\(([^)]*)\)(?: -> (\w+.*?))? \{`,逐行匹配。

- [ ] **Step 1: 生成器(std_doc.py)**

```python
#!/usr/bin/env python3
"""std_doc.py —— 从 std/*.ct 扫 pub fn 签名生成 website/docs/std/ 参考页(骨架,人补说明)"""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parent.parent
SIG = re.compile(r"^pub fn (\w+)\(([^)]*)\)(?:\s*->\s*(.+?))?\s*\{\s*$")
def render(mod: str, lines: list[str]) -> str:
    rows = []
    for m in SIG.finditer("\n".join(lines)):
        name, args, ret = m.group(1), m.group(2).strip(), (m.group(3) or "Unit").strip()
        rows.append(f"| `{name}({args})` | `{ret}` | 待补 |")
    body = "\n".join(rows) or "| — | — | 本模块暂无 pub fn |"
    return (f"# std.{mod}\n\n待补:模块用途一句话。\n\n"
            f"## pub fn\n\n| 签名 | 返回 | 说明 |\n|---|---|---|\n{body}\n")
def main() -> None:
    out = ROOT / "website/docs/std"
    out.mkdir(parents=True, exist_ok=True)
    mods = sorted(p.stem for p in (ROOT / "std").glob("*.ct"))
    index = ["# Std Reference\n\n", "标准库参考(骨架由 tools/std_doc.py 生成,说明人工补)。\n\n| 模块 | 页 |\n|---|---|\n"]
    for mod in mods:
        text = render(mod, (ROOT / f"std/{mod}.ct").read_text().splitlines())
        (out / f"{mod}.md").write_text(text)
        index.append(f"| std.{mod} | [{mod}.md]({mod}.md) |\n")
    (out / "README.md").write_text("".join(index))
    print(f"std_doc: {len(mods)} 模块页 → {out}")
if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: 生成与构建**

```bash
python3 tools/std_doc.py && cd website && .venv/bin/mkdocs build --strict
```
Expected: 20+ 模块页;strict 退出 0。

- [ ] **Step 3: 人工补首批说明(str/json/fs 三页的模块用途一句话,示范形态)**

- [ ] **Step 4: Commit**

```bash
git add tools/std_doc.py website/docs/std/
git commit -m "feat(site): std 参考生成器与 20+ 模块页——pub fn 签名表骨架,人补说明"
```

### Task 4: 五件套内容页

**Files:**
- Modify: `website/docs/index.md`、`index.zh.md`
- Create: `website/docs/getting-started.md`(+`.zh`)、`examples.md`(+`.zh`)、`download.md`(+`.zh`)

**要点(内容语风示例,正文各 40-80 行,以分发线事实为准):**
- 入门:安装(install.sh 一行/手动/源码 make 三法)→ hello(ctc new/run)→ build 出可执行 → 项目模式;run/check 零依赖、build 需 cc、Windows mingw。
- 下载:产物矩阵表(`ctron-<ver>-{darwin,linux}-{arm64,x86_64}.tar.gz`、`ctron-<ver>-windows-x86_64.zip`、`ctron-<ver>-src.tar.gz`、`SHA256SUMS`、`install.sh`,全部链 `https://github.com/Zturn/Ctron/releases/latest/download/<名>`)+ Releases 未发时首行"即将发布"。
- 示例:ctgrep/ctwc/ctwf 源码片段 + 复现命令。
- en 版:首页/入门/下载三页写真英文;examples 待翻占位。

- [ ] **Step 1: 写四件套内容(zh 先行,en 三页真英文)**
- [ ] **Step 2: strict 构建**
- [ ] **Step 3: Commit**

```bash
git add website/docs/index.md website/docs/index.zh.md website/docs/getting-started.* website/docs/examples.* website/docs/download.*
git commit -m "feat(site): 五件套内容——首页/入门/下载/示例(zh 全量,en 三页真英文)"
```

### Task 5: Pages workflow、分发线挂账修复与收口

**Files:**
- Create: `.github/workflows/pages.yml`
- Modify: `.github/workflows/release.yml`(publish 步补 `install.sh` 工件——分发线挂账:下载页与 install 一行指向 `releases/latest/download/install.sh`,但 publish 未上传它)
- Create: `tests/dist/site_check.sh`

- [ ] **Step 1: pages.yml**

```yaml
name: pages
on:
  push:
    branches: [main]
    paths: ['website/**', 'docs/spec/**', 'tools/sync_site_spec.sh', 'tools/std_doc.py']
  workflow_dispatch:
permissions: {contents: read, pages: write, id-token: write}
concurrency: {group: pages, cancel-in-progress: false}
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: {python-version: '3.x'}
      - run: pip install mkdocs-material mkdocs-static-i18n
      - name: spec 漂移检查
        run: sh tools/sync_site_spec.sh --check
      - name: 构建(strict)
        run: mkdocs build --strict
        working-directory: website
      - uses: actions/upload-pages-artifact@v3
        with: {path: website/site}
  deploy:
    needs: build
    runs-on: ubuntu-latest
    environment: {name: github-pages, url: '${{ steps.deployment.outputs.page_url }}'}
    steps:
      - {id: deployment, uses: actions/deploy-pages@v4}
```

- [ ] **Step 2: release.yml publish 补 install.sh**

publish 准备步(`sha256sum` 前)加一行:`cp install.sh rel/`(注释:网站下载页与 install 一行指向 latest/download/install.sh)。

- [ ] **Step 3: site_check.sh(本地全站验收)**

`tests/dist/site_check.sh`:①venv 存在则 strict 构建;②en 树完整断言(nav 每个目标的无后缀文件存在);③待翻注记断言(占位页含 `英文待翻`);④install 资产名与分发线口径断言(download 页八链接名逐一 grep);⑤YAML 解析两 workflow。退出码聚合。

- [ ] **Step 4: 验收**

```bash
sh tests/dist/site_check.sh
```
Expected: 全断言绿(网络不可用则 strict 构建步降级为 YAML+结构断言并记录)。

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/pages.yml .github/workflows/release.yml tests/dist/site_check.sh
git commit -m "feat(site): pages workflow(漂移检查+strict+deploy)+ release publish 补 install.sh 工件 + 全站验收脚本"
```
