#!/bin/sh
# examples/gui_calc/run.sh —— 仿 macOS 计算器综合示例
# 默认:构建 + headless 断言(CTRON_GUI_HEADLESS=1,注入点击/按键,无显示依赖,全自动)
# --run:追加启动真实窗口(交互验收:鼠标点按键;键盘 0-9 . + - * / = Enter ESC % 均可用)
# J19-④:链接咒语已吸收进 ctc build——清单 dependencies.gui 驱动自动链接
# (vendored raylib/freetype + ctron_gui 桥 + 平台框架参数),本文件零链接细节。
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$DIR")")
CTC="$ROOT/compiler/ctc.sh"
export CTRON_STDPATH="$ROOT/lib/std"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

sh "$ROOT/vendor/gui/build.sh" > /dev/null
"$CTC" build "$DIR/src/main.ct" -o "$T/gui_calc.bin" > /dev/null
echo "gui_calc: 构建+链接 OK(ctc build 自动链接)"

cd "$DIR"
CTRON_GUI_HEADLESS=1 "$T/gui_calc.bin"

if [ "${1:-}" = "--run" ]; then
    "$T/gui_calc.bin"
    echo "gui_calc: 窗口退出 rc=$?"
fi
