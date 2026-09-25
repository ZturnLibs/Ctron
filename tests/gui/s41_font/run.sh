# tests/gui/s41_font —— 波次五c:字体字重(§2.8;合成加粗宽差断言,须 FT 实测)
# tests/gui/s41_font —— 波次五a T2:图像管线(§2.6;加载/几何/拉伸/缓存命中/失败占位)
# style 直写 "#aa33cc" → RECT 命令 bg r/g/b = 170/51/204;d_cmd_bg_* 通路验证
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s41: 字重全绿" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh" > /dev/null

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
CTRON_STDPATH="$ROOT/std" "$EMIT" run "$DIR/src/main.ct" > "$T/s41.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux) FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s41: 字重全绿" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" -o "$T/s41.bin" \
   "$T/s41.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a" "$ROOT/vendor/gui/build/libraylib.a" $FW

cd "$DIR"
"$T/s41.bin"
echo "s41: 字重全绿"
