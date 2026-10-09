#!/bin/sh
# sync_site_spec.sh —— docs/spec → website/docs/spec/*.zh.md 单向同步(规范是活的,防 fork 漂移)
#   站点双语结构:无后缀 .md = 英文译件(手维护,不在本工具管辖);
#               .zh.md = 中文正典同步件(本工具独占写,--check 把关)。
#   无参:同步(覆盖);--check:重同步到临时目录与已提交件逐件 diff,漂移即非零退出
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$DIR/docs/spec"
DST="$DIR/website/docs/spec"
FM='<!-- 站点同步件(中文):源头 docs/spec/,勿直接编辑;漂移由 pages workflow --check 把关 -->
<!-- 英文正文 = 同名无后缀 .md(手维护译件,不随本工具再生) -->'

# 站点仅发布 website/docs/;指回仓库内部(tests/、docs/superpowers/)的相对链接在站点上永远断链,
# 改写为 GitHub 绝对链接。逐链接白名单:源新增外指链接时 strict 构建会拦下,届时在此补一行。
rewrite_repo_links() {
    sed \
        -e 's#](\.\./\.\./tests/#](https://github.com/ZturnLibs/Ctron/blob/main/tests/#g' \
        -e 's#](\.\./superpowers/specs/#](https://github.com/ZturnLibs/Ctron/blob/main/docs/superpowers/specs/#g'
}

# 对外发布面消毒(用户裁决 2026-10-09):站点不暴露内部规划/设计文档——
# 指向 docs/superpowers/** 的链接与路径一律替换为中性的"仓库内部设计文档",
# 只保留正文的规范性内容,不带内部文档坐标。
sanitize_internal_refs() {
    sed \
        -e 's#\[[^]]*\](https://github\.com/ZturnLibs/Ctron/blob/main/docs/superpowers/[^)]*)#仓库内部设计文档#g' \
        -e 's#`docs/superpowers/[^`]*`#仓库内部设计文档#g' \
        -e 's#docs/superpowers/[^)[:space:]`)]*#仓库内部设计文档#g' \
        -e 's#`[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}-[a-z0-9-]*\.md`#仓库内部设计文档#g' \
        -e 's#仓库内部设计文档 + 仓库内部设计文档#仓库内部设计文档#g'
}

if [ "${1:-}" = "--check" ]; then
    TMP=$(mktemp -d /tmp/sitespec.XXXXXX) && trap 'rm -rf "$TMP"' EXIT
    RC=0
    for F in "$SRC"/*.md; do
        B="$(basename "$F" .md).zh.md"
        { printf '%s\n' "$FM"; cat "$F"; } | rewrite_repo_links | sanitize_internal_refs > "$TMP/$B"
        if [ -f "$DST/$B" ]; then
            diff -u "$DST/$B" "$TMP/$B" || RC=1
        else
            echo "sync_site_spec: 站点缺中文件 $B(先跑无参同步)" >&2
            RC=1
        fi
    done
    for Z in "$DST"/*.zh.md; do
        B=$(basename "$Z" .zh.md)
        [ -f "$SRC/$B.md" ] || { echo "sync_site_spec: 陈旧同步件 $Z(源已删)" >&2; RC=1; }
    done
    [ "$RC" -eq 0 ] && echo "sync_site_spec: 无漂移 ✓"
    exit "$RC"
fi

mkdir -p "$DST"
for F in "$SRC"/*.md; do
    { printf '%s\n' "$FM"; cat "$F"; } | rewrite_repo_links | sanitize_internal_refs > "$DST/$(basename "$F" .md).zh.md"
done
# 陈旧件清理:源已删除的同步件一并移除(只清 .zh.md,英文手维护译件不动)
for OLD in "$DST"/*.zh.md; do
    B=$(basename "$OLD" .zh.md)
    [ -f "$SRC/$B.md" ] || rm -f "$OLD"
done
echo "sync_site_spec: 已同步 $(ls "$DST"/*.zh.md | wc -l | tr -d ' ') 件 → website/docs/spec/*.zh.md"
