# GUI flush 文本合流 FreeType 实施计划(方案 1)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Clay flush 的 TEXT 渲染从 raylib 默认位图字体(点阵观感、无 CJK)合流到域库 ft_shim FreeType 路径:实测测量接管 `ctron_measure`,字符串纹理 LRU 缓存供 flush 绘制,默认字体降级为无 CJK 字体环境下的兜底。

**Architecture:** `gui/c_src/ft_shim.c`(FreeType 单一真源)新增三个 C 口:`gui_ft_measure(_n)`(advance 求和实测宽)、`gui_ft_text`(白色 RGBA 渲染进 (串,px) LRU 缓存,槽位返回)、`gui_ft_text_draw`(懒上传纹理 + tint 上色绘制);探针复用现有 `ft_last_w/ft_probe_nonzero`(miss 渲染仍写 g_buf)。`gui/c_src/ctron_gui.c` 的 `ctron_measure` 换桥(实测失败或 `CTRON_GUI_FT_OFF=1` 时回 0.55 启发式),flush TEXT 分支优先走缓存路径。s16_measure 完成其注释预留的"上游合并"(删 fixture 本地 hook)。

**Tech Stack:** Ctron(.ct,emit 到 C)+ C(ctron_gui.c/ft_shim.c)+ Clay + raylib + FreeType(vendored,vendor/gui/build.sh 出静态库)。

## Global Constraints(记忆/仓库铁律)

- **并行机刷在飞**:`gui.ct`、`compiler/src/gui_parse.ct`、`compiler-rust/*`、`tests/roadmap/*`、e8_corpus 新文件是他泳道 WIP——**禁止触碰、禁止带入提交**;所有 `git add` 用 pathspec 限定。
- 验收必须 `ctron test` 全绿;不能用旧 seed bin(用 `compiler/bin/ctron-emit`)。
- 新 extern 契约 ≤8 参(ARM64 ≥9 参坑);本计划新 C 口最多 7 参。
- .ct 代码:禁 `;`;`&&`/`||` 有 or2 括号坑——测试代码用嵌套 if 规避。
- Clay `Clay_StringSlice.chars` **不以 NUL 结尾**(布局换行切片是子串视图)——新测量/渲染核心必须按 `(chars, len)` 循环,禁止 strlen 扫描;仅 FFI 边界(Str → char*)才是 NUL 串。
- 基线(2026-09-23,本机):ladder 25 过 / 6 败(s3_clay s4_events s5_golden e8_corpus w2_fold s18_scroll);e8_corpus 26 败确定属机刷 P-W2 extends WIP。**本计划验收 = 不新增红**,6 红归因见 Task 2 Step 0。
- 坐标黄金夹具(s5_golden/s21_frame_golden 等)对测量口径敏感:用 `CTRON_GUI_FT_OFF=1` 钉回 0.55 启发式,**不重录金框**,跨平台稳定(macOS Hiragino vs Linux Noto advance 不同)。
- flush 只在窗口循环(gui.ct `rt_window_loop*`)被调,headless 只走布局+命令断言;TEXT 绘制的 GL 上传无法 headless 覆盖,真窗视觉验收单列(Task 3,best-effort)。

## File Structure

| 文件 | 动作 | 职责 |
|---|---|---|
| `gui/c_src/ft_shim.c` | 修改 | 新增实测测量 + 白色渲染核心 `ft_render_n` + LRU 缓存 + 懒字体加载;`ft_render` 语义零变化 |
| `gui/c_src/ctron_gui.c` | 修改 | `ctron_measure` 换桥 + flush TEXT 分支走缓存路径(失败回退 DrawTextEx 默认字体) |
| `tests/gui/s28_ft_flush/` | 新建 | headless 夹具:实测≠启发式、缓存命中同槽、位图非空、布局盒宽==实测 |
| `tests/gui/s16_measure/` | 修改 | 上游合并:删 `c_src/measure_hook.c`,run.sh 链域库 ft_shim.c,断言口径不变 |
| `tests/gui/run.sh` + 25 个链接 `ctron_gui.c` 的 run.sh | 修改 | cc 行补 `ft_shim.c + libfreetype.a + -Ifreetype/include`;坐标黄金夹具补 `CTRON_GUI_FT_OFF=1` |
| `examples/gui_counter/app.ctml` + `src/main.ct` + `run.sh` | 修改 | 标签切中文「计数: 」,证声明式路径 CJK 一等 |
| `gui/README.md` | 修改 | 文本管线章节(合流后形态、回退链、链接面要求、FT_OFF 口径) |

---

### Task 1: ft_shim 实测测量 + 字符串纹理 LRU 缓存(TDD)

**Files:**
- Create: `tests/gui/s28_ft_flush/{run.sh,Ctron.ctcl,src/main.ct}`
- Modify: `gui/c_src/ft_shim.c`、`tests/gui/run.sh`(挂单 s28)

**Interfaces(produces,Task 2 依赖):**
- `int gui_ft_measure(const char* s, int px)` — NUL 串实测宽(s16/.ct FFI 口)
- `int gui_ft_measure_n(const char* s, int len, int px)` — 定长实测宽(ctron_measure 口);face 缺失/加载失败返回 -1
- `int gui_ft_text(const char* s, int len, int px)` — 缓存查找/渲染,返回槽位;失败 -1;miss 时渲染入 g_buf(白色,探针可读)——**仅供 C 侧 flush**
- `int gui_ft_text_str(const char* s, int px)` — NUL 串 2 参包装(.ct FFI 口;s28 用)。执行勘误:.ct extern 2 参对 C 3 参会 ABI 错位(px 被读成 len),与 ARM64 ≥9 参同族教训
- `int gui_ft_text_w(int slot)` — 槽宽;`int gui_ft_text_draw(int slot, int x, int y, int r, int g, int b, int a)` — 懒上传 + tint 绘制(GL)
- 执行勘误:ft_shim 的 4 参 `gui_clear` 与 ctron_gui.c 的 3 参 `gui_clear` 撞 C 符号(此前无夹具同链两文件)——直绘版改名 `ft_gui_clear`,gui_cjk/s9 两处调用同步

- [ ] **Step 1: 写失败测试 s28_ft_flush**

`tests/gui/s28_ft_flush/Ctron.ctcl`:
```
pkg {
    manifest_version = 1
    name = "gui_s28_ft_flush"
    version = "0.1.0"
}
```

`tests/gui/s28_ft_flush/src/main.ct`:
```ct
// tests/gui/s28_ft_flush —— M3 合流第一验:域库 ft 实测测量 + 字符串纹理 LRU 缓存(headless)
// 断言口径:①实测宽 ≠ 0.55 启发式;②miss 位图非空(ft_probe_nonzero 复用);③同串同槽命中;
// ④布局 TEXT 盒宽 == gui_ft_measure 实测(证 ctron_measure 换桥,Task 2 后全绿)。
// 无 CJK 字体即红,与 s8 同口径(阶梯隐含 CJK 字体前提)。

#[trusted]
extern "c" fn gui_ft_measure(s: Str, px: I32) -> I32

#[trusted]
extern "c" fn gui_ft_text(s: Str, px: I32) -> I32

#[trusted]
extern "c" fn gui_ft_text_w(slot: I32) -> I32

#[trusted]
extern "c" fn ft_last_w() -> I32

#[trusted]
extern "c" fn ft_probe_nonzero() -> I32

#[trusted]
extern "c" fn gui_clay_init(w: I32, h: I32) -> I32

#[trusted]
extern "c" fn gui_begin_layout(w: I32, h: I32) -> I32

#[trusted]
extern "c" fn gui_open() -> I32

#[trusted]
extern "c" fn gui_close() -> I32

#[trusted]
extern "c" fn gui_cfg(dir: I32, gap: I32, padx: I32, pady: I32, ax: I32, ay: I32,
                      wmode: I32, wval: I32, hmode: I32, hval: I32,
                      rr: I32, gg: I32, bb: I32, aa: I32) -> I32

#[trusted]
extern "c" fn gui_text(s: Str, size: I32, r: I32, g: I32, b: I32, a: I32) -> I32

#[trusted]
extern "c" fn gui_end_layout() -> I32

#[trusted]
extern "c" fn gui_cmd_count() -> I32

#[trusted]
extern "c" fn gui_cmd_type(i: I32) -> I32

#[trusted]
extern "c" fn gui_cmd_w100(i: I32) -> I32

fn main() -> I32 {
    // ① 实测:宽为正,且 ≠ 0.55 启发式(12 字节 CJK 串)
    var m: I32 = gui_ft_measure("中文测试", 24)
    var heur: I32 = 12 * 24 * 55 / 100
    if m <= 0 {
        println("s28: 实测宽度非正(字体未加载?)")
        return 1
    }
    if m == heur {
        println("s28: 实测宽撞上 0.55 启发式(桥未接管)")
        return 1
    }
    // ② miss 渲染:位图非空,纹理宽 ≥ 实测宽(pen+8 填充)
    var s1: I32 = gui_ft_text("中文测试", 24)
    if s1 < 0 {
        println("s28: 首渲槽位异常")
        return 1
    }
    if ft_probe_nonzero() <= 0 {
        println("s28: 位图为空")
        return 1
    }
    if ft_last_w() < m {
        println("s28: 纹理宽小于实测宽")
        return 1
    }
    // ③ 缓存:同串同槽;异串异槽;槽宽为正
    var s2: I32 = gui_ft_text("中文测试", 24)
    if s1 != s2 {
        println("s28: 缓存未命中(同串不同槽)")
        return 1
    }
    var s3: I32 = gui_ft_text("另一串文本", 24)
    if s3 == s1 {
        println("s28: 不同串同槽(缓存键错)")
        return 1
    }
    if gui_ft_text_w(s3) <= 0 {
        println("s28: 槽宽非正")
        return 1
    }
    // ④ 布局换桥:TEXT 盒宽 ×100 == 实测 ×100
    gui_clay_init(320, 200)
    gui_begin_layout(320, 200)
    gui_open()
    gui_cfg(0, 8, 16, 16, 0, 0, 2, 320, 2, 200, 0, 0, 0, 0)
    gui_text("中文测试", 24, 235, 235, 240, 255)
    gui_close()
    var n: I32 = gui_end_layout()
    if n < 1 {
        println("s28: 布局无命令")
        return 1
    }
    var i: I32 = 0
    var found: I32 = 0
    while i < n {
        if gui_cmd_type(i) == 1 {
            assert_eq(gui_cmd_w100(i), m * 100)
            found = 1
        }
        i += 1
    }
    if found == 0 {
        println("s28: 无 TEXT 命令")
        return 1
    }
    println("s28 ft_flush: 实测测量+纹理缓存全绿(headless)")
    return 0
}
```

`tests/gui/s28_ft_flush/run.sh`(照 s8 模式,链 ctron_gui.c + ft_shim.c):
```sh
#!/bin/sh
# tests/gui/s28_ft_flush/run.sh —— M3 合流:域库 ft 实测测量 + 纹理缓存 headless 断言
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(dirname "$(dirname "$(dirname "$DIR")")")
EMIT="$ROOT/compiler/bin/ctron-emit"
[ -x "$EMIT" ] || { echo "s28: 缺 compiler/bin/ctron-emit" >&2; exit 2; }

sh "$ROOT/vendor/gui/build.sh"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$EMIT" run "$DIR/src/main.ct" > "$T/s28.c"

case "$(uname)" in
    Darwin) FW="-framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo" ;;
    Linux)  FW="-lX11 -lGL -lm -lpthread -ldl" ;;
    *) echo "s28: unsupported platform" >&2; exit 1 ;;
esac

cc -O1 -w -I"$ROOT/vendor/gui/clay" -I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include" \
   -o "$T/s28.bin" \
   "$T/s28.c" "$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" \
   "$ROOT/vendor/gui/build/libraylib.a" "$ROOT/vendor/gui/build/libfreetype.a" $FW
"$T/s28.bin"
echo "s28_ft_flush: 实测测量+纹理缓存全绿(headless)"
```

- [ ] **Step 2: 跑 s28 验证失败**

Run: `sh tests/gui/s28_ft_flush/run.sh`
Expected: **链接失败**,`gui_ft_measure`/`gui_ft_text`/`gui_ft_text_w` 未定义符号(Task 1 实现前,④ 还不会跑;此时断言 ①②③ 也因符号缺失不可达)。

- [ ] **Step 3: 实现 ft_shim.c 新增**

在 `ft_shim.c` 追加(位置:ft_probe_nonzero 之后、纹理注册表之前):

```c
// ---- M3 合流:实测测量 + 字符串纹理 LRU 缓存(flush 口径;ctron_gui.c 调用) ----
// 白色 RGBA 入缓存,颜色在 gui_ft_text_draw 用 tint 上色——缓存键 = (串,px)。
// miss 渲染写 g_buf:探针 ft_last_w/ft_probe_nonzero 原口径可用(s28)。
// CTRON_GUI_FT_OFF=1 的启发式回退在 ctron_gui.c 的 ctron_measure 侧,本文件不读 env。
static int g_ft_failed = 0;
static int g_ft_px = 0;

static int ft_ensure(int px) {
    if (g_ft_failed) { return -1; }
    if (!g_face) {
        if (ft_load_cjk(24) != 0) { g_ft_failed = 1; return -1; }
        g_ft_px = 24;
    }
    if (px > 0 && px != g_ft_px) {
        FT_Set_Pixel_Sizes(g_face, 0, (FT_UInt)px);
        g_ft_px = px;
    }
    return 0;
}

// 定长 UTF-8 步进(Clay StringSlice 非 NUL 结尾,必须按 len 扫)
static unsigned long utf8_next_n(const unsigned char* s, int len, int* i) {
    if (*i >= len) { return 0; }
    unsigned char c = s[*i];
    if (c < 0x80) { (*i)++; return c; }
    int n = (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
    if (*i + n > len) { *i = len; return 0; }
    unsigned long cp = (unsigned long)(c & (unsigned char)(0xFF >> (n + 1)));
    int k = 1;
    while (k < n) {
        cp = (cp << 6) | (unsigned long)(s[*i + k] & 0x3F);
        k += 1;
    }
    *i += n;
    return cp;
}

// 实测宽:与 ft_render_n 同一迭代(FT_Load_Char),保证测量==渲染 advance 完全一致
static int ft_measure_n(const char* s, int len, int px) {
    if (ft_ensure(px) != 0) { return -1; }
    int w = 0;
    int i = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)s, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        w += (int)(g_face->glyph->advance.x >> 6);
    }
    return w;
}
int gui_ft_measure(const char* s, int px) {
    return ft_measure_n(s, (int)strlen(s), px);
}
int gui_ft_measure_n(const char* s, int len, int px) {
    return ft_measure_n(s, len, px);
}

#define FT_TEXT_CACHE 64
typedef struct {
    char* str;
    int len;
    int px;
    unsigned char* buf;
    int w;
    int h;
    Texture2D tex;
    int uploaded;
    long use;
} FtText;
static FtText g_tc[FT_TEXT_CACHE];
static int g_tc_n = 0;
static long g_tc_clock = 0;

int gui_ft_text(const char* s, int len, int px) {
    if (ft_ensure(px) != 0) { return -1; }
    g_tc_clock++;
    for (int i = 0; i < g_tc_n; i++) {
        if (g_tc[i].px == px && g_tc[i].len == len &&
            memcmp(g_tc[i].str, s, (size_t)len) == 0) {
            g_tc[i].use = g_tc_clock;
            return i;
        }
    }
    if (ft_render_n(s, len, 255, 255, 255) != 0) { return -1; }
    int slot;
    if (g_tc_n < FT_TEXT_CACHE) {
        slot = g_tc_n++;
    } else {
        slot = 0;
        for (int i = 1; i < FT_TEXT_CACHE; i++) {
            if (g_tc[i].use < g_tc[slot].use) { slot = i; }
        }
        if (g_tc[slot].uploaded) { UnloadTexture(g_tc[slot].tex); g_tc[slot].uploaded = 0; }
        free(g_tc[slot].str);
        free(g_tc[slot].buf);
    }
    FtText* e = &g_tc[slot];
    e->len = len;
    e->px = px;
    e->str = (char*)malloc((size_t)len);
    memcpy(e->str, s, (size_t)len);
    e->w = g_bw;
    e->h = g_bh;
    e->buf = (unsigned char*)malloc((size_t)g_bw * g_bh * 4);
    memcpy(e->buf, g_buf, (size_t)g_bw * g_bh * 4);
    e->uploaded = 0;
    e->use = g_tc_clock;
    return slot;
}

int gui_ft_text_w(int slot) {
    if (slot < 0 || slot >= g_tc_n) { return -1; }
    return g_tc[slot].w;
}

int gui_ft_text_draw(int slot, int x, int y, int r, int g, int b, int a) {
    if (slot < 0 || slot >= g_tc_n) { return -1; }
    FtText* e = &g_tc[slot];
    if (!e->uploaded) {
        Image img = { 0 };
        img.data = e->buf;
        img.width = e->w;
        img.height = e->h;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        e->tex = LoadTextureFromImage(img);
        e->uploaded = 1;
    }
    DrawTexture(e->tex, x, y, (Color){ (unsigned char)r, (unsigned char)g,
                                       (unsigned char)b, (unsigned char)a });
    return 0;
}
```

同时把 `ft_render` 重构为定长核心(语义零变化,`utf8_next` 原样保留给既有路径不动;`ft_render_n` 内部循环改用 `utf8_next_n` + len):

```c
static int ft_render_n(const char* utf8, int len, int r, int g, int b) {
    if (!g_face) { return -1; }
    int asc = g_face->size->metrics.ascender >> 6;
    int hgt = (int)((g_face->size->metrics.height >> 6)) + 4;
    int i = 0;
    int pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)utf8, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_DEFAULT) != 0) { continue; }
        pen += (int)(g_face->glyph->advance.x >> 6);
    }
    g_bw = pen + 8;
    g_bh = hgt;
    g_buf = (unsigned char*)realloc(g_buf, (size_t)g_bw * g_bh * 4);
    memset(g_buf, 0, (size_t)g_bw * g_bh * 4);
    i = 0;
    pen = 0;
    while (i < len) {
        unsigned long cp = utf8_next_n((const unsigned char*)utf8, len, &i);
        if (cp == 0) { break; }
        if (FT_Load_Char(g_face, (FT_ULong)cp, FT_LOAD_RENDER) != 0) { continue; }
        FT_GlyphSlot sl = g_face->glyph;
        int bx = pen + sl->bitmap_left;
        int by = asc - sl->bitmap_top;
        int bw = (int)sl->bitmap.width;
        int bh = (int)sl->bitmap.rows;
        for (int gy = 0; gy < bh; gy++) {
            for (int gx = 0; gx < bw; gx++) {
                int px = bx + gx;
                int py = by + gy;
                if (px < 0 || py < 0 || px >= g_bw || py >= g_bh) { continue; }
                unsigned char a = sl->bitmap.buffer[(size_t)gy * bw + gx];
                unsigned char* q = &g_buf[((size_t)py * g_bw + px) * 4];
                q[0] = (unsigned char)r; q[1] = (unsigned char)g; q[2] = (unsigned char)b;
                if (a > q[3]) { q[3] = a; }
            }
        }
        pen += (int)(sl->advance.x >> 6);
    }
    return 0;
}

int ft_render(const char* utf8, int r, int g, int b) {
    return ft_render_n(utf8, (int)strlen(utf8), r, g, b);
}
```

⚠️ 注:`ft_render_n` 需前置声明放在 `gui_ft_text` 之前;两遍宽度循环与 `ft_measure_n` 同迭代序(测量==渲染)。注意 C 是顺序编译,把 `ft_render_n` 放在 `ft_measure_n` 之前。

- [ ] **Step 4: ladder 挂单**

`tests/gui/run.sh` 的 for 列表 `s27_checkbox_d` 后追加 ` s28_ft_flush`。

- [ ] **Step 5: 跑 s28 → 绿;跑 s8_cjk + s16 + gui_cjk + gui_counter 冒烟 → 无回归**

Run: `sh tests/gui/s28_ft_flush/run.sh && sh tests/gui/s8_cjk/run.sh && sh tests/gui/s16_measure/run.sh && sh examples/gui_cjk/run.sh && sh examples/gui_counter/run.sh`
Expected: 全部 OK。⚠️ s28 断言 ④ 此时应已绿:`ctron_measure` 还是 0.55 桩,`m != heur` 仍成立(CJK 12 字节下 FT 实测≠启发式),`gui_cmd_w100 == m*100` 要求 measure 已换桥——**若 ④ 红,把它挪到 Task 2 验证,本任务只保 ①②③**(执行时按实际口径记录)。实际预期:④ 红(盒宽=0.55 桩值 ≠ FT 实测)→ 本步验收改为:①②③ 绿,④ 断言以 `if gui_cmd_w100(i) != m * 100` 输出诊断后 `return 1` 改为 Task 2 再放开——即 s28 在 Task 1 允许红 ④,阶梯挂单放到 Task 2。**执行口径:Task 1 不改 ladder run.sh;Task 2 放开 ④ 后再挂单。**

- [ ] **Step 6: 提交(pathspec 限定)**

```bash
git add gui/c_src/ft_shim.c tests/gui/s28_ft_flush
git commit -m "feat(gui): ft_shim 实测测量+字符串纹理 LRU 缓存 API——flush 合流前置片(s28 断言 ①②③ 绿,④ 随换桥放开)"
```

### Task 2: ctron_gui 换桥 + flush 合流 + s16 上游合并 + 链接面

**Files:**
- Modify: `gui/c_src/ctron_gui.c`(measure 换桥 + flush TEXT 分支)
- Modify: `tests/gui/s16_measure/`(删本地 hook,链域库)
- Modify: 25 个链接 `ctron_gui.c` 的 run.sh(补 ft_shim/libfreetype/-I)——examples: gui_calc/gui_counter/todo;tests: s3 s4 s5 s6 s7 s10 s11 s12 s13 s16 s17 s18 s19 s20 s21 s22 s23 s24 s25 s26 s27 w4_ctml_reload
- Modify: `tests/gui/run.sh`(挂单 s28)+ 坐标黄金夹具 run.sh 补 `CTRON_GUI_FT_OFF=1`

**Interfaces(consumes Task 1):** `gui_ft_measure_n` / `gui_ft_text` / `gui_ft_text_draw`(见 Task 1 签名)。

- [ ] **Step 0: 六红基线归因(干净 worktree 对照,确认与文本路径无关)**

```bash
git worktree add /tmp/ctron-clean HEAD
cd /tmp/ctron-clean/compiler-rust && cargo build --release
# 按 compiler-rust 的构建产物装配 /tmp/ctron-clean/compiler/bin/ctron-emit(参照仓库既有构建脚本)
cd /tmp/ctron-clean && for s in s3_clay s4_events s5_golden w2_fold s18_scroll; do sh tests/gui/$s/run.sh > /tmp/clean_$s.log 2>&1; echo "$s rc=$?"; done
```
Expected: 六红在干净 HEAD 同样红 → 属他泳道/既有债;若某夹具干净下绿 → 该红是机刷 WIP 态交互,同样不属本计划,登记即可。**两种结果都不阻塞,记录归因表。**(worktree 用完 `git worktree remove /tmp/ctron-clean`)

- [ ] **Step 1: ctron_gui.c 换桥**

文件头(`#include` 后)加前置声明:

```c
// ft_shim.c(M3 合流):实测测量 + 字符串纹理缓存;链接面须含 ft_shim.c + libfreetype
extern int gui_ft_measure_n(const char* s, int len, int px);
extern int gui_ft_text(const char* s, int len, int px);
extern int gui_ft_text_draw(int slot, int x, int y, int r, int g, int b, int a);
```

`ctron_measure` 替换为:

```c
// 测量:FreeType 实测接管;CTRON_GUI_FT_OFF=1 → 0.55 启发式(坐标黄金夹具钉值,跨平台稳定);
// 字体缺失(g_ft_failed)同样回启发式——无 CJK 字体环境行为与旧版一致
static int gui_ft_off = -1;

static Clay_Dimensions ctron_measure(Clay_StringSlice text, Clay_TextElementConfig *cfg, void *ud) {
    (void)ud;
    if (gui_ft_off < 0) { gui_ft_off = getenv("CTRON_GUI_FT_OFF") ? 1 : 0; }
    int w = -1;
    if (!gui_ft_off) {
        w = gui_ft_measure_n(text.chars, (int)text.length, (int)cfg->fontSize);
    }
    if (w < 0) {
        w = (int)((float)text.length * (float)cfg->fontSize * 0.55f);
    }
    return (Clay_Dimensions){ (float)w, (float)cfg->fontSize * 1.25f };
}
```

flush 的 `CLAY_RENDER_COMMAND_TYPE_TEXT` 分支替换为:

```c
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_StringSlice s = c->renderData.text.stringContents;
                Clay_Color col = c->renderData.text.textColor;
                int slot = gui_ft_text(s.chars, (int)s.length, (int)c->renderData.text.fontSize);
                if (slot >= 0) {
                    gui_ft_text_draw(slot, (int)b.x, (int)b.y,
                                     (int)col.r, (int)col.g, (int)col.b, (int)col.a);
                } else {
                    // 兜底:无 CJK 字体环境回默认位图字体(ASCII 界面仍可用,即旧观感)
                    DrawTextEx(GetFontDefault(), s.chars, (Vector2){ b.x, b.y },
                               (float)c->renderData.text.fontSize, 0.0f,
                               (Color){ (unsigned char)col.r, (unsigned char)col.g,
                                        (unsigned char)col.b, (unsigned char)col.a });
                }
                break;
            }
```

- [ ] **Step 2: s16 上游合并**

- `git rm tests/gui/s16_measure/c_src/measure_hook.c`
- `tests/gui/s16_measure/src/main.ct`:删 `gui_measure_install`/`gui_measure_hook` 两个 extern 与 main 里对应两行调用(其余断言口径不动——`gui_ft_measure` 现在解析到域库 ft_shim.c)
- `tests/gui/s16_measure/run.sh`:cc 行删 `"$DIR"/c_src/measure_hook.c`,加 `"$ROOT/gui/c_src/ft_shim.c"` 与 `-I"$ROOT/vendor/gui/freetype/include"`、`"$ROOT/vendor/gui/build/libfreetype.a"`
- 头注释补一句:「上游已合并(M3):测量桥=域库 ft_shim,本夹具仅验证」

- [ ] **Step 3: 链接面批量补齐(25 个 run.sh)**

```bash
for f in examples/gui_calc/run.sh examples/gui_counter/run.sh examples/todo/run.sh \
         tests/gui/s3_clay/run.sh tests/gui/s4_events/run.sh tests/gui/s5_golden/run.sh \
         tests/gui/s6_demo/run.sh tests/gui/s7_window/run.sh tests/gui/s10_when/run.sh \
         tests/gui/s11_each/run.sh tests/gui/s12_input/run.sh tests/gui/s13_item/run.sh \
         tests/gui/s17_bidi_layout/run.sh tests/gui/s18_scroll/run.sh tests/gui/s19_input_d/run.sh \
         tests/gui/s20_embed/run.sh tests/gui/s21_frame_golden/run.sh tests/gui/s22_sk_equiv/run.sh \
         tests/gui/s23_sk_native/run.sh tests/gui/s24_model_d/run.sh tests/gui/s25_expr_d/run.sh \
         tests/gui/s26_reload_d/run.sh tests/gui/s27_checkbox_d/run.sh tests/gui/w4_ctml_reload/run.sh; do
  # include 行补 freetype(紧随 raylib include)
  sed -i '' 's|-I"$ROOT/vendor/gui/raylib"|-I"$ROOT/vendor/gui/raylib" -I"$ROOT/vendor/gui/freetype/include"|' "$f"
  # cc 行补 ft_shim + libfreetype(紧随 ctron_gui.c)
  sed -i '' 's|"\$ROOT/gui/c_src/ctron_gui.c"|"$ROOT/gui/c_src/ctron_gui.c" "$ROOT/gui/c_src/ft_shim.c" "$ROOT/vendor/gui/build/libfreetype.a"|' "$f"
done
grep -L "ft_shim.c" examples/*/run.sh tests/gui/*/run.sh | grep -v s8_cjk | grep -v s9_window_cjk | grep -v s28 || echo "链接面齐"
```
Expected: 最后一行输出 `链接面齐`(s8/s9/s28 本就链 ft_shim)。逐文件形态有差异者(如 gui_counter 的 build() 函数体、s16)人工核对 sed 结果。

- [ ] **Step 4: 跑 s28 → 四断言全绿,挂单 ladder**

放开/确认 ④(`assert_eq(gui_cmd_w100(i), m * 100)`),然后:
`sh tests/gui/s28_ft_flush/run.sh` → 绿;`tests/gui/run.sh` for 列表 `s27_checkbox_d` 后追加 ` s28_ft_flush`。

- [ ] **Step 5: 全 ladder → 修 FT_OFF 名单**

Run: `sh tests/gui/run.sh`
Expected: 25+1 过(s28)/ ≤6 败(基线六红不新增)。对**基线绿、现在红**的坐标/黄金夹具:其 run.sh 加 `export CTRON_GUI_FT_OFF=1`(build/emit 之前)后复跑转绿;预期名单 s5_golden、s21_frame_golden、s17_bidi_layout(s18_scroll 本就红,同样加上钉值防复发)。若某夹具加 FT_OFF 仍红 → 其红与本测量口径无关,按归因表登记。

- [ ] **Step 6: 示例冒烟 + ctron test 全量**

`sh examples/gui_calc/run.sh && sh examples/gui_counter/run.sh && sh examples/todo/run.sh 2>/dev/null || true`(todo 无 run.sh 时记录即可;以实际存在者为准)
Run: `ctron test`
Expected: 全绿(验收铁律)。

- [ ] **Step 7: 提交(pathspec 限定,不触机刷文件)**

```bash
git add gui/c_src/ctron_gui.c gui/c_src/ft_shim.c \
  tests/gui/s16_measure tests/gui/s28_ft_flush tests/gui/run.sh \
  examples/gui_calc/run.sh examples/gui_counter/run.sh examples/todo/run.sh \
  tests/gui/s3_clay/run.sh tests/gui/s4_events/run.sh tests/gui/s5_golden/run.sh \
  tests/gui/s6_demo/run.sh tests/gui/s7_window/run.sh tests/gui/s10_when/run.sh \
  tests/gui/s11_each/run.sh tests/gui/s12_input/run.sh tests/gui/s13_item/run.sh \
  tests/gui/s17_bidi_layout/run.sh tests/gui/s18_scroll/run.sh tests/gui/s19_input_d/run.sh \
  tests/gui/s20_embed/run.sh tests/gui/s21_frame_golden/run.sh tests/gui/s22_sk_equiv/run.sh \
  tests/gui/s23_sk_native/run.sh tests/gui/s24_model_d/run.sh tests/gui/s25_expr_d/run.sh \
  tests/gui/s26_reload_d/run.sh tests/gui/s27_checkbox_d/run.sh tests/gui/w4_ctml_reload/run.sh
git commit -m "feat(gui): Clay flush TEXT 合流 FreeType——实测测量接管 ctron_measure,位图字体降为兜底;s16 上游合并;链接面补 ft_shim/freetype;坐标黄金夹具 FT_OFF 钉值"
```

### Task 3: 声明式中文示范(gui_counter)+ 文档 + 真窗视觉验收

**Files:**
- Modify: `examples/gui_counter/app.ctml`、`src/main.ct`、`run.sh`
- Modify: `gui/README.md`

- [ ] **Step 1: gui_counter 标签切中文**

`app.ctml`:`<label>count: {count}</label>` → `<label>计数: {count}</label>`;第 2 行注释改为:「窗口文本走 FreeType 纹理缓存(M3 合流后 CJK 一等);直绘路径见 examples/gui_cjk」。
`src/main.ct`:所有 `d_expect_text(t, "count: " + ...)` 的 `"count: "` 字面量同步为 `"计数: "`(含状态恢复往返内的断言)。
`run.sh`:grep 口径检查——`grep "count=0"` 是状态键名不受影响;若有按显示文本 grep 的行同步改。

- [ ] **Step 2: gui_counter 全流程绿**

Run: `sh examples/gui_counter/run.sh`
Expected: 构建+headless 断言+状态恢复往返全 OK(显示文本断言已是中文)。

- [ ] **Step 3: gui/README.md 文本管线章节**

追加一节(放在绘制/flush 相关段落后):
- flush TEXT → `gui_ft_text` LRU 缓存((串,px) 键,白色 RGBA,tint 上色,容量 64)→ `gui_ft_text_draw` 懒上传;
- `ctron_measure` = FreeType 实测;回退链:无 CJK 字体或 `CTRON_GUI_FT_OFF=1` → 0.55 启发式;
- 链接面要求:凡链 `gui/c_src/ctron_gui.c` 的 run.sh 须加 `gui/c_src/ft_shim.c + vendor/gui/build/libfreetype.a + -Ifreetype/include`;
- 直绘路径(ft_render/ft_tex_*)不变,gui_cjk 仍为直绘示范。

- [ ] **Step 4: 真窗视觉验收(best-effort,如实记录)**

```bash
cd examples/gui_counter && CTRON_GUI_STATE=count=7 sh run.sh --run &
sleep 4 && screencapture -x /tmp/gui_counter_cjk.png && kill %1
```
看图确认:中文非问号、无点阵块状感、布局无重叠。若 screencapture 权限不可用,如实登记「视觉验收未自动化」,以 s28 headless 断言 + gui_cjk 既有视觉背书收尾。

- [ ] **Step 5: 终验 + 提交**

Run: `sh tests/gui/run.sh && ctron test`
Expected: 同 Task 2 口径(不新增红)。然后:

```bash
git add examples/gui_counter gui/README.md
git commit -m "feat(gui): gui_counter 标签切中文——声明式路径 CJK 一等示范;gui README 文本管线章节(M3 合流)"
```

---

## Self-Review 记录

- 覆盖:flush 合流 ✓ 测量接管 ✓ LRU ✓ 回退链 ✓ s16 合并 ✓ 探针保留 ✓ 头less 可测(GAAP:GL 上传不可 headless,Task 3 视觉补)✓
- 占位符:无(所有代码/命令已给全;Step 3 的 sed 逐文件核对是显式步骤)
- 类型一致:`gui_ft_measure(Str,I32)->I32` 与 s16 既有 extern 完全一致;s28 用 I32 形参(s16 同);C 口 7 参封顶 ✓
- 风险已显式化:s28 ④ 依赖换桥(Task 1/2 边界口径见 Task 1 Step 5);FT_OFF 名单经验法则闭环(Step 5);六红基线归因(Step 0)
