#!/bin/sh
# site_check.sh —— 全站本地验收(网站文档线 Task 5 Step 3;用法:sh tests/dist/site_check.sh,仓库内任意 cwd)
#
# 断言清单(退出码聚合:除 [5] 降级外,任一 FAIL → 退出 1):
#   [1] en 树完整   —— mkdocs.yml nav 每个目标的无后缀件在 website/docs/ 下存在(i18n suffix 结构:en 无后缀/zh 带 .zh;
#                      nav 含 Language Spec 嵌套子节,解析不限缩进层)
#   [2] 待翻注记    —— en 占位页含「英文待翻」标记(现存占位:examples.md + spec/ 同步 11 件,后者由 sync FM 注入);
#                      反向:真英文页(index/getting-started/download)不得被占位覆盖
#   [3] 下载页八资产 —— download.md/download.zh.md 对八资产名逐一 grep;en 页 latest/download 直链
#                      唯一名恰为八;并与 release.yml 分发口径对照(四平台 + windows zip + src +
#                      SHA256SUMS + install.sh 发布补行)
#   [4] workflow YAML —— pages.yml / release.yml 均可 yaml.safe_load;pages.yml 关键步骤在位
#   [5] strict 构建  —— website/.venv 在则 mkdocs build --strict 真构建(输出至临时目录,零副作用);
#                      不在则降级为 DEGRADED 记录(不计败;网络不可用致 venv 缺依赖同此口径)
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WEB="$ROOT/website"
DOCS="$WEB/docs"
REL=".github/workflows/release.yml"
PW=".github/workflows/pages.yml"

PASS_N=0
FAIL_N=0
ok()   { PASS_N=$((PASS_N + 1)); echo "  PASS: $1"; }
bad()  { FAIL_N=$((FAIL_N + 1)); echo "  FAIL: $1"; }
degr() { echo "  DEGRADED: $1"; }

BLD=""
cleanup() { [ -n "$BLD" ] && rm -rf "$BLD"; return 0; }
trap cleanup EXIT

echo "== [1] en 树完整(nav 全目标无后缀件:顶层 5 页 + spec 11 件) =="
NAV=$(sed -n '/^nav:/,$p' "$WEB/mkdocs.yml" | sed -n 's/^[[:space:]]*- .*:[[:space:]]*//p' | tr -d '"')
if [ -z "$NAV" ]; then
    bad "mkdocs.yml nav 解析为空"
fi
NAV_N=0
for T in $NAV; do
    NAV_N=$((NAV_N + 1))
    if [ -f "$DOCS/$T" ]; then ok "docs/$T 存在(nav 目标)"; else bad "docs/$T 缺失(nav 断链)"; fi
done
if [ "$NAV_N" -eq 16 ]; then ok "nav 目标数 16/16(顶层 5 + spec 11)"; else bad "nav 目标数 $NAV_N ≠ 16"; fi

echo "== [2] 待翻注记 =="
if grep -q '英文待翻' "$DOCS/examples.md"; then
    ok "占位页 examples.md 含「英文待翻」注记"
else
    bad "examples.md 为 en 占位页但缺「英文待翻」注记"
fi
SPEC_N=0
for S in "$DOCS"/spec/*.md; do
    SPEC_N=$((SPEC_N + 1))
    if grep -q '英文待翻' "$S"; then
        ok "spec 同步件含「英文待翻」注记:$(basename "$S")"
    else
        bad "spec 同步件缺「英文待翻」注记:$(basename "$S")"
    fi
done
if [ "$SPEC_N" -eq 11 ]; then ok "spec/ 同步件数 11/11 均查注记"; else bad "spec/ 同步件数 $SPEC_N ≠ 11"; fi
for REAL in index.md getting-started.md download.md; do
    if grep -q '英文待翻' "$DOCS/$REAL"; then
        bad "$REAL 应为真英文页,却含占位注记(被占位覆盖?)"
    else
        ok "真英文页 $REAL 无占位注记"
    fi
done

echo "== [3] 下载页七资产 与 分发线口径对照 =="
ASSETS="ctron-0.0.1-darwin-arm64.tar.gz \
ctron-0.0.1-linux-x86_64.tar.gz ctron-0.0.1-linux-arm64.tar.gz \
ctron-0.0.1-windows-x86_64.zip ctron-0.0.1-src.tar.gz SHA256SUMS install.sh"
for A in $ASSETS; do
    if grep -qF "$A" "$DOCS/download.md" && grep -qF "$A" "$DOCS/download.zh.md"; then
        ok "七资产之一在 en/zh 下载页:$A"
    else
        bad "资产 $A 在下载页缺位(en/zh 至少其一)"
    fi
done
# en 下载页 latest/download 直链唯一名恰为七(资产名字符集;`<asset>` 占位写法被字符集滤空)
GOT=$(grep -o 'releases/latest/download/[A-Za-z0-9._-]*' "$DOCS/download.md" | sed 's|.*/||' | sort -u)
NGOT=$(printf '%s\n' "$GOT" | grep -c .)
if [ "$NGOT" -eq 7 ]; then
    ok "en 下载页 latest/download 直链唯一名恰 7 个"
else
    bad "en 下载页直链唯一名 $NGOT ≠ 7"
fi
for A in $ASSETS; do
    printf '%s\n' "$GOT" | grep -qxF "$A" || bad "直链集合缺 $A(页面直链与资产矩阵漂移)"
done
# 分发线口径:release.yml 能产出下载页承诺的每一件
for P in darwin-arm64 darwin-x86_64 linux-x86_64 linux-arm64 windows-x86_64; do
    if grep -q "$P" "$ROOT/$REL"; then ok "release.yml 覆盖平台 $P"; else bad "release.yml 缺平台 $P 口径"; fi
done
grep -q 'src.tar.gz' "$ROOT/$REL" && ok "release.yml 覆盖 src 包" || bad "release.yml 缺 src 包口径"
grep -q 'SHA256SUMS' "$ROOT/$REL" && ok "release.yml 覆盖 SHA256SUMS" || bad "release.yml 缺 SHA256SUMS 口径"
if grep -q 'cp install.sh rel/' "$ROOT/$REL"; then
    ok "release.yml publish 补行 cp install.sh rel/(第八资产,本任务修复)"
else
    bad "release.yml publish 未上传 install.sh(下载页/install 一行将 404)"
fi

echo "== [4] workflow YAML 解析 =="
PYBIN=""
for CAND in "$WEB/.venv/bin/python3" "$WEB/.venv/bin/python" "$(command -v python3 || true)" "$(command -v python || true)"; do
    [ -n "$CAND" ] && [ -x "$CAND" ] || continue
    "$CAND" -c 'import yaml' >/dev/null 2>&1 || continue
    PYBIN=$CAND
    break
done
if [ -z "$PYBIN" ]; then
    bad "无带 PyYAML 的解释器(可 website/.venv 或 pip install pyyaml),无法解析 workflow"
else
    PYOUT=$(cd "$ROOT" && "$PYBIN" - 2>&1 <<'PYEOF'
import yaml

for f in (".github/workflows/pages.yml", ".github/workflows/release.yml"):
    with open(f, encoding="utf-8") as fh:
        yaml.safe_load(fh)
    print("PASS: YAML safe_load " + f)
PYEOF
)
    YRC=$?
    printf '%s\n' "$PYOUT"
    [ "$YRC" -eq 0 ] || bad "workflow YAML 解析失败(见上)"
fi
grep -q 'mkdocs build --strict' "$ROOT/$PW" && ok "pages.yml 含 strict 构建" || bad "pages.yml 缺 strict 构建"
grep -q 'sync_site_spec.sh --check' "$ROOT/$PW" && ok "pages.yml 含 spec 漂移门" || bad "pages.yml 缺 spec 漂移门"
grep -q 'upload-pages-artifact@v3' "$ROOT/$PW" && ok "pages.yml 含 upload-pages-artifact" || bad "pages.yml 缺 upload-pages-artifact"
grep -q 'deploy-pages@v4' "$ROOT/$PW" && ok "pages.yml 含 deploy-pages" || bad "pages.yml 缺 deploy-pages"

echo "== [5] strict 构建(venv 在则真构建) =="
if [ -x "$WEB/.venv/bin/mkdocs" ]; then
    BLD=$(mktemp -d "${TMPDIR:-/tmp}/site_check_build.XXXXXX")
    BOUT=$(cd "$WEB" && ./.venv/bin/mkdocs build --strict -d "$BLD" 2>&1)
    BRC=$?
    if [ "$BRC" -eq 0 ]; then
        ok "mkdocs build --strict(venv;输出 → $BLD,已随退出清理)"
    else
        printf '%s\n' "$BOUT"
        bad "mkdocs build --strict 失败(rc=$BRC)"
    fi
else
    degr "website/.venv 不存在,strict 构建降级为 [1]-[4] 结构断言(记录,不计败)"
fi

echo "== 汇总 =="
echo "PASS=$PASS_N FAIL=$FAIL_N"
if [ "$FAIL_N" -ne 0 ]; then
    echo "site_check: 红 ✗"
    exit 1
fi
echo "site_check: 全部断言绿 ✓"
exit 0
