#!/bin/sh
# tests/gui/s74_ctcnew/run.sh —— J19-④⑤:ctc new --gui 脚手架 + ctc build 自动链接 e2e
# 脚手架(pkg+gui.entry+dependencies.gui+双路模板)→ 构建(清单驱动 GUI 域库
# 自动链接,零手工链接参数)→ headless 冒烟断言。全程临时目录,零仓库污染。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
CTC="$ROOT/compiler/ctc.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
sh "$ROOT/vendor/gui/build.sh" > /dev/null

"$CTC" new "$T/demo" --gui > /dev/null
[ -f "$T/demo/Ctron.ctcl" ] || { echo "s74: 清单缺席" >&2; exit 1; }
[ -f "$T/demo/app.ctml" ] || { echo "s74: 入口 ctml 缺席" >&2; exit 1; }
"$CTC" build "$T/demo/src/main.ct" -o "$T/demo/app" > /dev/null

cd "$T/demo"
CTRON_GUI_HEADLESS=1 ./app run src/main.ct
echo "s74: ctc new --gui + build 自动链接全绿"
