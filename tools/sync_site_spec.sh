#!/bin/sh
# sync_site_spec.sh —— docs/spec → website/docs/spec 单向同步(规范是活的,防 fork 漂移)
#   无参:同步(覆盖);--check:重同步到临时目录与已提交件 diff,漂移即非零退出
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$DIR/docs/spec"
DST="$DIR/website/docs/spec"
FM='<!-- 站点同步件:源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->
<!-- 英文待翻:中文占位 -->'

# 站点仅发布 website/docs/;指回仓库内部(tests/、docs/superpowers/)的相对链接在站点上永远断链,
# 改写为 GitHub 绝对链接。逐链接白名单:源新增外指链接时 strict 构建会拦下,届时在此补一行。
rewrite_repo_links() {
    sed \
        -e 's#](\.\./\.\./tests/#](https://github.com/ZturnLibs/Ctron/blob/main/tests/#g' \
        -e 's#](\.\./superpowers/specs/#](https://github.com/ZturnLibs/Ctron/blob/main/docs/superpowers/specs/#g'
}

if [ "${1:-}" = "--check" ]; then
    TMP=$(mktemp -d /tmp/sitespec.XXXXXX) && trap 'rm -rf "$TMP"' EXIT
    for F in "$SRC"/*.md; do
        { printf '%s\n' "$FM"; cat "$F"; } | rewrite_repo_links > "$TMP/$(basename "$F")"
    done
    diff -r "$DST" "$TMP"
    echo "sync_site_spec: 无漂移 ✓"
    exit 0
fi

mkdir -p "$DST"
for F in "$SRC"/*.md; do
    { printf '%s\n' "$FM"; cat "$F"; } | rewrite_repo_links > "$DST/$(basename "$F")"
done
# 陈旧件清理:源已删除的同步件一并移除(--check 的 Only in 亦可拦,此处免手工)
for OLD in "$DST"/*.md; do
    B=$(basename "$OLD")
    [ -f "$SRC/$B" ] || rm -f "$OLD"
done
echo "sync_site_spec: 已同步 $(ls "$DST" | wc -l | tr -d ' ') 件 → website/docs/spec/"
