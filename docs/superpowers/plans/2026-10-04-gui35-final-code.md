# GUI-35 终形代码存档(2026-10-04;发射器 let-insts 段错误修复后回施)
# 回施步骤:
# 1) pkgs/gui/gui_render.ct 尾追加 ax_role_of(下 block A)
# 2) pkgs/gui/gui_driver.ct 于 rt_async_drain 前插入 d_ax_dump+ax_dump_walk(下 block B)
# 3) 门面 gui.ct:gui_render 组+ax_role_of;gui_driver 组+d_ax_dump, ax_dump_walk
#    (⚠️ pub extern 带参 E2020 怪相在册——本件无 extern,纯 fn 安全)
# 4) tests/gui/s90_a11y(夹具已在本仓 tests/gui/s90_a11y,主仓未跟踪副本可删)
# 5) sh tests/gui/s90_a11y/run.sh → 六断言(三 role/两文本/bind 路径)

## block A(gui_render.ct 尾追加):
pub fn ax_role_of(tag: Str) -> Str {
    if tag == "button" {
        return "button"
    }
    if tag == "input" {
        return "textbox"
    }
    if tag == "textarea" {
        return "textbox"
    }
    if tag == "checkbox" {
        return "checkbox"
    }
    if tag == "label" {
        return "text"
    }
    return "generic"
}

## block B(gui_driver.ct 插入):
pub fn d_ax_dump(t: Box[Driver]) -> Str {
    return ax_dump_walk(t.t, 0)
}

fn ax_dump_walk(t: GuiTree, id: I32, depth: I32) -> Str {
    var pad: Str = ""
    var pi: I32 = 0
    while pi < depth {
        pad = pad + "  "
        pi += 1
    }
    var role: Str = ax_role_of(t.ntag[id])
    var line: Str = pad + role + " " + t.ntag[id]
    var txt: Str = t.npre[id]
    if t.nbid[id] != "" {
        line = line + " bind=" + t.nbid[id]
    }
    if txt != "" {
        line = line + " '" + txt + "'"
    }
    var nl: Str = utf8_enc(10)
    var acc: Str = line + nl
    var ch: I32 = t.nfc[id]
    while ch >= 0 {
        acc = acc + ax_dump_walk(t, ch, depth + 1)
        ch = t.ns[ch]
    }
    return acc
}
