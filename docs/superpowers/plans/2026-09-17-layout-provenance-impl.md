# 布局原因链 prov-v0(S8)实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 2026-09-17 规格(`2026-09-17-layout-provenance-design.md`)§5 的 prov-v0——shim 影子树记录 + 布局后置推导 + 自检断言 + headless dump 黄金差分,新夹具 `tests/gui/s8_prov/` 全绿;同步完成 roadmap 登记与两份文档回填。

**Architecture:** shim(C 侧)在既有 `gui_open/gui_cfg/gui_text/gui_close` 调用序列上维护影子树(dev 开关 `gui_prov_enable` 控制,零回调);`Clay_EndLayout` 后按方案 A 后置推导每节点 W/H/X/Y 的 rule+inputs+result,并与命令缓冲回填的最终几何做自检断言(偏差即 abort);dump 渲染进静态缓冲,经 `gui_prov_text_len/text_byte` 逐字节读回,Ctron 夹具与黄金文件差分。W3(E8191/E8192/E8193 检查面)与 M2(浮层原语 + inspector)**不在本计划**,仅登记 roadmap,各自待第二波/独立计划。

**Tech Stack:** Ctron(compiler/bin/ctron-emit 解释/发射)、vendored Clay v0.14 + raylib 5.5(`vendor/gui/`)、夹具自包含 `c_src/ctron_gui.c` shim、黄金差分。

## Global Constraints

- shim 边界一律 I32(裸 F32 extern 缺口未修复):几何经 ×100 整数读回,尺寸整数像素;
- 零回调:prov 只新增**查询缝**(dump 读回),不新增任何 Ctron→C 回调;
- 元素编号 = root 0、子自 1(S6 已实证的事件表同款约定);
- prov 全部 dev-only:由 `gui_prov_enable()` 显式开启,旧夹具 s1–s7 不开启则零行为变化;
- Clay 钉 v0.14、raylib 钉 5.5(§12.4),升级走黄金快照回归;
- 主线程 only(§6.2 线程模型),prov 数据帧级生命周期;
- v0 推导简化口径(如实记录于 dump 与本计划,不算偏离):GROW 按"单 grow 子 + 兄弟 fixed"计算;交叉轴 wrap/scroll 规则(PR_WRAP_NL/PR_SCROLL_OFF)v0 不产出;自检仅覆盖 **W/H 两轴**且仅覆盖**发绘制命令的节点**(TEXT ↔ 文本节点按序对齐,RECTANGLE ↔ 带背景节点按序对齐);
- 夹具运行口径:S5 发现——`run` 的 read_file 锚用**默认烘焙路径**,run.sh 须 `cd` 夹具目录;
- 诊断码 E8191/E8192/E8193 已在 16 日规格 §4.4 登记,本计划不新增码。

---

### Task 1: roadmap 登记 GUI 泳道增量

**Files:**
- Modify: `docs/superpowers/plans/2026-09-13-spec-completion-roadmap.md`(文件末尾追加)

**Interfaces:**
- Consumes: 无
- Produces: roadmap 中的「GUI 泳道」小节(W3/M2 增量的唯一登记处,后续计划引用)

- [ ] **Step 1: 在文件末尾追加小节**

```markdown
---

## GUI 泳道增量(2026-09-17 设计会话并入)

> 上游 spec:`docs/superpowers/specs/2026-09-17-layout-provenance-design.md`;
> 引擎对齐裁决(暴露面 = Clay v0.14 能力面)已落 16 日规格 §5.1。

- **S8 prov-v0(随 GUI 阶梯,当前)**:布局原因链记录层——shim 影子树 + 后置推导 +
  自检断言 + headless dump 黄金差分(spec §5;原 §8 "S5.5" 因 S5/S6 已完成重编号为 S8)。
- **W3 增补(第二波,编译器集成)**:E8191 静态溢出 / E8192 死 fill(编译期静态验证,
  spec §7)+ E8193 each-无-key-含输入控件警告;负例语料进 `tests/09_gui/`。
- **M2 增补**:浮层原语公共化(16 日规格 §5.1 引擎对齐)+ inspector 停靠/浮动双模式
  (spec §6)——浮层原语为 inspector 前置,独立计划排程。
```

- [ ] **Step 2: 提交**

```bash
git commit docs/superpowers/plans/2026-09-13-spec-completion-roadmap.md -m "docs(roadmap): GUI 泳道增量登记——S8 prov-v0 / W3 E8191-3 / M2 浮层+inspector"
```

---

### Task 2: s8_prov 夹具骨架 + shim 影子树记录

**Files:**
- Create: `tests/gui/s8_prov/`(自 `tests/gui/s6_demo/` 整体拷贝)
- Modify: `tests/gui/s8_prov/c_src/ctron_gui.c`(追加 prov 块)
- Modify: `tests/gui/run.sh`(追加 s8 一行,格式照抄 s6 行)

**Interfaces:**
- Consumes: s6 的全部 shim 缝(`gui_open/gui_cfg/gui_text/gui_close/gui_end_layout/gui_cmd_*100/gui_cmd_text_*/gui_inject_*`)
- Produces: `gui_prov_enable()`、影子树 `g_pn[]`(后续 Task 3/4 消费);`gui_fail()`(夹具断言失败出口)

- [ ] **Step 1: 拷贝夹具并改名**

```bash
cp -r tests/gui/s6_demo tests/gui/s8_prov
sed -i '' 's/s6/s8/g' tests/gui/s8_prov/run.sh
```

Windows/Linux 下 sed -i 参数按平台调整;目标是 run.sh 内所有 `s6` 字面量变 `s8`。确认 `tests/gui/run.sh` 里 s6 的调用行格式,在其后照抄一行 `s8`。

- [ ] **Step 2: 写失败断言(Ctron 侧)**

在 `tests/gui/s8_prov/src/main.ct` 的既有 `use` 区追加(签名风格对照该文件内既有 extern 声明;若 bind 块为独立快照文件则改该文件):

```ct
#[trusted] extern fn gui_prov_enable()
#[trusted] extern fn gui_fail()
```

在主循环首次布局完成后插入:

```ct
gui_prov_enable()
```

- [ ] **Step 3: 跑一次确认失败(链接错误)**

```bash
sh tests/gui/s8_prov/run.sh
```

预期:链接失败 `Undefined symbols: _gui_prov_enable`(或等价报错),rc != 0。

- [ ] **Step 4: shim 追加 prov 记录层**

在 `tests/gui/s8_prov/c_src/ctron_gui.c` 末尾追加(并在 `gui_open/gui_cfg/gui_text/gui_close/gui_end_layout` 五个既有函数体内各加一行 hook,位置见块尾注释):

```c
// ---- prov(2026-09-17 spec §5;S8)—— dev-only:影子树 + 记录 ----
#define PROV_MAX 64
#define PROV_IN_MAX 4
enum { PR_NONE = 0, PR_FIXED, PR_HUG_TEXT, PR_HUG_CHILD, PR_GROW,
       PR_LINE_START, PR_PREV_SIB };
enum { PI_STYLE = 0, PI_TEXT_MEASURE, PI_PARENT_CONTENT, PI_SIBLING_FIXED,
       PI_GAP, PI_PADDING };
typedef struct { int kind, value; } ProvIn;
typedef struct { int rule, result, nin; ProvIn in[PROV_IN_MAX]; } ProvAxis;
typedef struct {
    int parent, first_child, next_sibling;      /* root=0、子自 1(S6 约定) */
    int dir, gap, padx, pady, wmode, wval, hmode, hval;
    int text_chars, text_size;                  /* -1 = 非文本节点 */
    int has_bg;                                 /* aa > 0,RECTANGLE 命令按序对齐用 */
    int fin_w, fin_h, fin_x, fin_y;             /* ×100,命令缓冲回填;-1 = 未知 */
    ProvAxis w, h, x, y;                        /* ax_ 前置清零见 prov_reset */
} ProvNode;
static ProvNode g_pn[PROV_MAX];
static int g_pn_n = 0, g_prov_on = 0;
static int g_prov_par[PROV_MAX], g_prov_sp = 0;
static char g_pdump[4096]; static int g_pdump_n = 0;

void gui_prov_enable(void) { g_prov_on = 1; }
void gui_fail(void) { fprintf(stderr, "s8: assert failed\n"); exit(1); }

static ProvNode *prov_cur(void) {
    if (!g_prov_on || g_prov_sp == 0) return 0;
    return &g_pn[g_prov_par[g_prov_sp - 1]];
}
```

五个 hook(guard 全部为 `g_prov_on`,未开启零开销):

```c
/* gui_open 末尾: */
if (g_prov_on) {
    if (g_pn_n >= PROV_MAX) { fprintf(stderr, "prov: overflow\n"); abort(); }
    ProvNode *n = &g_pn[g_pn_n];
    memset(n, 0, sizeof *n);
    n->text_chars = -1; n->text_size = -1;
    n->first_child = -1; n->next_sibling = -1;
    n->fin_w = n->fin_h = n->fin_x = n->fin_y = -1;
    n->parent = (g_prov_sp > 0) ? g_prov_par[g_prov_sp - 1] : -1;
    if (n->parent >= 0) {
        ProvNode *p = &g_pn[n->parent];
        if (p->first_child < 0) p->first_child = g_pn_n;
        else { int c = p->first_child;
               while (g_pn[c].next_sibling >= 0) c = g_pn[c].next_sibling;
               g_pn[c].next_sibling = g_pn_n; }
    }
    g_prov_par[g_prov_sp++] = g_pn_n;
    g_pn_n++;
}
/* gui_cfg 末尾: */
{ ProvNode *n = prov_cur();
  if (n) { n->dir = dir; n->gap = gap; n->padx = padx; n->pady = pady;
           n->wmode = wmode; n->wval = wval; n->hmode = hmode; n->hval = hval;
           n->has_bg = (aa > 0); } }
/* gui_text 末尾(strlen(s) 与 Clay_String 长度同源): */
{ ProvNode *n = prov_cur();
  if (n) { n->text_chars = (int)strlen(s); n->text_size = size; } }
/* gui_close 末尾: */
if (g_prov_on && g_prov_sp > 0) g_prov_sp--;
/* gui_end_layout 内、g_cmd_count 赋值之后: */
if (g_prov_on) { prov_backfill(); prov_derive(); prov_assert(); prov_dump_render(); }
```

- [ ] **Step 5: 跑夹具确认恢复绿(prov 只记录不推导,dump 为空不致断言)**

本步先不给 `prov_backfill/derive/assert/dump_render` 之外的行为——Task 2 末尾先放四个空实现(`static void prov_backfill(void) {}` 等),使构建链接通过、夹具原有断言全绿:

```bash
sh tests/gui/s8_prov/run.sh
```

预期:输出 s8 全绿,rc = 0(影子树在记录,尚未消费)。

- [ ] **Step 6: 提交**

```bash
git add tests/gui/s8_prov tests/gui/run.sh
git commit -m "feat(gui): S8 prov 影子树记录层——gui_open/cfg/text/close 五缝 dev-only 记录"
```

---

### Task 3: 后置推导 + 自检断言(C 侧)

**Files:**
- Modify: `tests/gui/s8_prov/c_src/ctron_gui.c`(实现 `prov_backfill/prov_derive/prov_assert`,替换空实现)

**Interfaces:**
- Consumes: Task 2 的 `g_pn[]`(cfg/文本测量已记录)、命令缓冲 `g_cmds`
- Produces: 每节点 `w/h/x/y` 的 rule+inputs+result;`fin_*` 回填;自检 abort 保证后续 dump 数据可信

- [ ] **Step 1: 写失败断言(先在 main.ct 加几何锚)**

main.ct 首帧布局后插入(数字按"root 固定宽 340、内含 label+button 的 s6 树"预期几何;若夹具树不同,按 s6 黄金几何换算——断言的目的是让 Task 3 的回填+自检有事可查):

```ct
#[trusted] extern fn gui_cmd_count() -> I32
#[trusted] extern fn gui_cmd_w100(i: I32) -> I32
#[trusted] extern fn gui_cmd_text_len(i: I32) -> I32

fn assert_ge(a: I32, b: I32) {
    if a < b {
        gui_fail()
    }
}
// 首帧后: 至少一条 TEXT 命令且其宽为正(证明回填通路活着)
assert_ge(gui_cmd_count(), 1)
assert_ge(gui_cmd_text_len(0), 1)
```

注:`gui_cmd_count/gui_cmd_w100` 若 bind 快照已有则不重复声明。先跑 `sh tests/gui/s8_prov/run.sh`——若当前空实现下断言已过,本步的"红"体现为 Task 3 Step 3 的 prov 断言强度;FFI 层 TDD 以"断言先落、实现后到"为口径。

- [ ] **Step 2: 实现 prov_backfill(命令缓冲几何回填)**

```c
static void prov_backfill(void) {
    int ti = 0, ri = 0;   /* TEXT / RECTANGLE 命令序号 ↔ 文本 / 带背景节点按序对齐 */
    for (int i = 0; i < g_pn_n; i++) {
        ProvNode *n = &g_pn[i];
        if (n->text_chars >= 0) {
            /* 找第 ti 个 TEXT 命令 */
            while (ti < g_cmd_count && gui_cmd_type_raw(ti) != 1) ti++;
            if (ti < g_cmd_count) {
                n->fin_x = gui_cmd_x100_raw(ti); n->fin_y = gui_cmd_y100_raw(ti);
                n->fin_w = gui_cmd_w100_raw(ti); ti++;
            }
        } else if (n->has_bg) {
            while (ri < g_cmd_count && gui_cmd_type_raw(ri) != 2) ri++;
            if (ri < g_cmd_count) {
                n->fin_x = gui_cmd_x100_raw(ri); n->fin_y = gui_cmd_y100_raw(ri);
                n->fin_w = gui_cmd_w100_raw(ri); ri++;
            }
        }
    }
}
```

其中 `gui_cmd_type_raw/x100_raw/y100_raw/w100_raw` 为既有 `gui_cmd_type/gui_cmd_x100/...` 的内部静态版(直接读 `cmd(i)->`,不经导出层;在 prov 块前加 `static int gui_cmd_type_raw(int i)` 等四个薄封装,实现拷自既有导出函数)。S3 黄金几何口径一致:文本 (16,16)、测宽 = 字符数 × 字号 × 0.55、测高 = 字号 × 1.25。

- [ ] **Step 3: 实现 prov_derive(W 显式,H 同构,X/Y 最小集)**

```c
static void ax_set(ProvAxis *a, int rule, int result) {
    a->rule = rule; a->result = result; a->nin = 0;
}
static void ax_in(ProvAxis *a, int kind, int value) {
    if (a->nin < PROV_IN_MAX) { a->in[a->nin].kind = kind; a->in[a->nin].value = value; a->nin++; }
}

static void prov_derive(void) {
    for (int i = 0; i < g_pn_n; i++) {
        ProvNode *n = &g_pn[i];
        /* ---- W ---- */
        if (n->wmode == 2) {
            ax_set(&n->w, PR_FIXED, n->wval);
            ax_in(&n->w, PI_STYLE, n->wval);
        } else if (n->text_chars >= 0) {
            int mw = (int)(n->text_chars * n->text_size * 0.55f);
            ax_set(&n->w, PR_HUG_TEXT, mw);
            ax_in(&n->w, PI_TEXT_MEASURE, mw);
        } else if (n->wmode == 1 && n->parent >= 0) {
            ProvNode *p = &g_pn[n->parent];
            int cnt = 0, fixed_sum = 0;
            for (int c = p->first_child; c >= 0; c = g_pn[c].next_sibling) {
                cnt++;
                if (c != i && g_pn[c].wmode == 2) fixed_sum += g_pn[c].wval;
            }
            int gaps = (cnt > 1) ? (cnt - 1) * p->gap : 0;
            int base = (p->wmode == 2) ? p->wval : 0;
            int free_w = base - fixed_sum - gaps - 2 * p->padx;
            if (free_w < 0) free_w = 0;
            ax_set(&n->w, PR_GROW, free_w);
            ax_in(&n->w, PI_PARENT_CONTENT, base);
            ax_in(&n->w, PI_SIBLING_FIXED, fixed_sum);
            ax_in(&n->w, PI_GAP, gaps);
            ax_in(&n->w, PI_PADDING, 2 * p->padx);
        } else if (n->first_child >= 0 && n->fin_w >= 0) {
            ax_set(&n->w, PR_HUG_CHILD, n->fin_w / 100);
            ax_in(&n->w, PI_SIBLING_FIXED, n->fin_w / 100);
        }
        /* ---- H(与 W 同构:hmode/hval/pady/fin_h;文本高 = size*1.25) ---- */
        if (n->hmode == 2) {
            ax_set(&n->h, PR_FIXED, n->hval);
            ax_in(&n->h, PI_STYLE, n->hval);
        } else if (n->text_chars >= 0) {
            int mh = n->text_size * 125 / 100;
            ax_set(&n->h, PR_HUG_TEXT, mh);
            ax_in(&n->h, PI_TEXT_MEASURE, mh);
        } else if (n->hmode == 1 && n->parent >= 0) {
            ProvNode *p = &g_pn[n->parent];
            int cnt = 0, fixed_sum = 0;
            for (int c = p->first_child; c >= 0; c = g_pn[c].next_sibling) {
                cnt++;
                if (c != i && g_pn[c].hmode == 2) fixed_sum += g_pn[c].hval;
            }
            int gaps = (cnt > 1) ? (cnt - 1) * p->gap : 0;
            int base = (p->hmode == 2) ? p->hval : 0;
            int free_h = base - fixed_sum - gaps - 2 * p->pady;
            if (free_h < 0) free_h = 0;
            ax_set(&n->h, PR_GROW, free_h);
            ax_in(&n->h, PI_PARENT_CONTENT, base);
            ax_in(&n->h, PI_SIBLING_FIXED, fixed_sum);
            ax_in(&n->h, PI_GAP, gaps);
            ax_in(&n->h, PI_PADDING, 2 * p->pady);
        }
        /* ---- X/Y(最小集:首子 = LINE_START(pad),次子起 = PREV_SIB) ---- */
        if (n->parent >= 0) {
            ProvNode *p = &g_pn[n->parent];
            int prev = -1;
            for (int c = p->first_child; c >= 0 && c != i; c = g_pn[c].next_sibling) prev = c;
            if (prev < 0) {
                ax_set(&n->x, PR_LINE_START, p->padx);
                ax_in(&n->x, PI_PADDING, p->padx);
                ax_set(&n->y, PR_LINE_START, p->pady);
                ax_in(&n->y, PI_PADDING, p->pady);
            } else if (g_pn[prev].fin_w >= 0 && g_pn[prev].fin_x >= 0
                       && n->fin_x >= 0) {
                ax_set(&n->x, PR_PREV_SIB,
                       g_pn[prev].fin_x / 100 + g_pn[prev].fin_w / 100 + p->gap);
                ax_in(&n->x, PI_SIBLING_FIXED, g_pn[prev].fin_w / 100);
                ax_in(&n->x, PI_GAP, p->gap);
            }
        }
    }
}
```

- [ ] **Step 4: 实现 prov_assert(自检:推导 ≠ 实际即 dev panic)**

```c
static void prov_assert(void) {
    for (int i = 0; i < g_pn_n; i++) {
        ProvNode *n = &g_pn[i];
        /* 只断言有命令缓冲几何的轴(×100 空间,容差 0.5px = 50) */
        if (n->fin_w >= 0 && n->w.rule != PR_NONE &&
            (n->w.result * 100 - n->fin_w) * (n->w.result * 100 - n->fin_w) > 50 * 50) {
            fprintf(stderr, "prov: el%d W derive=%d actual=%d\n",
                    i, n->w.result, n->fin_w / 100);
            abort();
        }
        if (n->fin_h >= 0 && n->h.rule != PR_NONE &&
            (n->h.result * 100 - n->fin_h) * (n->h.result * 100 - n->fin_h) > 50 * 50) {
            fprintf(stderr, "prov: el%d H derive=%d actual=%d\n",
                    i, n->h.result, n->fin_h / 100);
            abort();
        }
    }
}
```

注:HUG_CHILD 节点的 result 取自 fin(回填值),自检恒真,属预期(其真输入是子几何);GROW 在 min/max 约束或多 grow 子场景会被此断言当场拦下——正是 spec 裁决 2 的目的。

- [ ] **Step 5: 跑夹具确认绿**

```bash
sh tests/gui/s8_prov/run.sh
```

预期:rc = 0 无 `prov:` 报错。若 abort,按打印的 el/轴修推导分支(常见:root 无 wmode=2 时 base 取 0 导致 GROW=0——此时 root 应显式 fixed,golden 夹具树保证这一点)。

- [ ] **Step 6: 提交**

```bash
git add tests/gui/s8_prov
git commit -m "feat(gui): S8 prov 后置推导+自检断言——W/H 规则分类 + 命令缓冲几何逐帧核对"
```

---

### Task 4: dump 渲染 + 读回缝 + 黄金差分

**Files:**
- Modify: `tests/gui/s8_prov/c_src/ctron_gui.c`(实现 `prov_dump_render` + 三个读回导出)
- Modify: `tests/gui/s8_prov/src/main.ct`(黄金差分断言)
- Create: `tests/gui/s8_prov/golden/prov_dump.txt`(首跑生成,人工核对后冻结)

**Interfaces:**
- Consumes: Task 3 的 `g_pn[]` 推导结果
- Produces: `gui_prov_text_len() -> I32`、`gui_prov_text_byte(i: I32) -> I32`、`gui_prov_golden_write()`(Ctron extern);黄金文件 `golden/prov_dump.txt`

- [ ] **Step 1: shim 实现 dump 与读回**

```c
static void dump_axis(int i, const char *ax, ProvAxis *a) {
    if (a->rule == PR_NONE) return;
    g_pdump_n += snprintf(g_pdump + g_pdump_n, sizeof g_pdump - g_pdump_n,
                          "el%d %s rule=%d res=%d", i, ax, a->rule, a->result);
    for (int k = 0; k < a->nin; k++)
        g_pdump_n += snprintf(g_pdump + g_pdump_n, sizeof g_pdump - g_pdump_n,
                              " in%d=%d", a->in[k].kind, a->in[k].value);
    g_pdump_n += snprintf(g_pdump + g_pdump_n, sizeof g_pdump - g_pdump_n, "\n");
}

static void prov_dump_render(void) {
    g_pdump_n = 0;
    for (int i = 0; i < g_pn_n; i++) {
        dump_axis(i, "W", &g_pn[i].w);
        dump_axis(i, "H", &g_pn[i].h);
        dump_axis(i, "X", &g_pn[i].x);
        dump_axis(i, "Y", &g_pn[i].y);
    }
}

int gui_prov_text_len(void) { return g_pdump_n; }
int gui_prov_text_byte(int i) {
    if (i < 0 || i >= g_pdump_n) return -1;
    return (unsigned char)g_pdump[i];
}
void gui_prov_golden_write(void) {
    const char *p = getenv("S8_WRITE_GOLDEN");
    if (!p) return;
    FILE *f = fopen("golden/prov_dump.txt", "wb");
    if (!f) { fprintf(stderr, "prov: golden open failed\n"); exit(1); }
    fwrite(g_pdump, 1, g_pdump_n, f);
    fclose(f);
}
```

(`<stdio.h>/<stdlib.h>/<string.h>` 该文件已含;确定性由测高启发式与遍历序保证。)

- [ ] **Step 2: Ctron 侧接黄金差分断言**

main.ct 增 extern 与读文件(`read_file` 走 S5 默认烘焙锚,路径相对夹具目录):

```ct
#[trusted] extern fn gui_prov_text_len() -> I32
#[trusted] extern fn gui_prov_text_byte(i: I32) -> I32
#[trusted] extern fn gui_prov_golden_write()
```

首帧布局后、既有事件循环前:

```ct
// gui_prov_enable() 已在 Task 2 接入,勿重复
gui_prov_golden_write()   // 仅 S8_WRITE_GOLDEN=1 时落盘
var gl: Str = read_file("golden/prov_dump.txt")
var n: I32 = gui_prov_text_len()
if n != gl.len {
    gui_fail()
}
var i: I32 = 0
while i < n {
    if gui_prov_text_byte(i) != byte_at(gl, i) {
        gui_fail()
    }
    i = i + 1
}
```

- [ ] **Step 3: 生成并人工核对黄金 v1**

```bash
cd tests/gui/s8_prov && mkdir -p golden
S8_WRITE_GOLDEN=1 sh run.sh
cat golden/prov_dump.txt
```

Expected(形态示意,具体数字随夹具树):每行 `el<i> <轴> rule=<码> res=<n> in<k>=<v>…`,首行类似 `el0 W rule=1 res=340 in0=0 in...=340`。人工核对:root 为 FIXED(340);label 行 HUG_TEXT(measure = 字节数×字号×0.55);grow 子的 GROW 输入含 parent.content/sibling/gap/padding 四项。

- [ ] **Step 4: 冻结黄金跑绿**

```bash
sh tests/gui/s8_prov/run.sh
sh tests/gui/run.sh
```

Expected:s8 全绿;`tests/gui/run.sh` 七旧夹具 + s8 连续全绿(黄金被篡改一字节即 rc=1——可手动验证一次后还原)。

- [ ] **Step 5: 提交**

```bash
git add tests/gui/s8_prov tests/gui/run.sh
git commit -m "feat(gui): S8 prov dump 读回缝 + 黄金差分——逐字节原因链快照进夹具"
```

---

### Task 5: 文档回填(阶梯 + 规格重编号)

**Files:**
- Modify: `docs/superpowers/plans/2026-09-16-gui-mvp-ladder.md`(阶梯图追加 S8 行 + 已执行勾选)
- Modify: `docs/superpowers/specs/2026-09-17-layout-provenance-design.md`(§8 里程碑表 S5.5 → S8 注记;§9 回填清单勾选)

**Interfaces:**
- Consumes: Task 2–4 的完成事实
- Produces: 文档与实现一致

- [ ] **Step 1: 阶梯计划追加 S8**

在「泳道阶梯 S0–S6」节末尾(S7 之后)追加:

```markdown
### S8 prov-v0 布局原因链(2026-09-17 增补;原 09-17 规格 §8 "S5.5" 因 S5/S6 已完成重编号)

- 产出:`tests/gui/s8_prov/`——shim 影子树 + 后置推导 + 自检断言 + dump 黄金差分
  (spec §5 方案 A;dev-only,`gui_prov_enable` 门控);
- DoD:s8 夹具全绿;黄金 `golden/prov_dump.txt` 冻结;全阶梯回归含 s8 九夹具绿。
```

执行完成后在「本会话已执行」清单按既有格式追加 `- [x] **S8** …` 行。

- [ ] **Step 2: 09-17 规格 §8/§9 回填**

§8 表首行 `| **S5.5** prov-v0(插入 MVP 阶梯 S5/S6 间,~1 天) | …` 改为:

```markdown
| **S8** prov-v0(原编 S5.5;S5/S6 已先行完成故重编号,~1 天) | 影子树 + 后置推导 + 自检断言 + `gui_prov_dump` 黄金差分 | S3 桥(已完成) |
```

§9 第三条改为:

```markdown
- `2026-09-16-gui-mvp-ladder.md` 增补 S8 行(原 S5.5,重编号;已落地)。
```

同规格 §5 接口命名随实现回写:`gui_prov_dump(node)` 的实现形态 = **全树 dump 读回**(`gui_prov_text_len/text_byte` 逐字节)——窄接口纪律下 per-node 寻址无必要,§5 措辞一并修订。

- [ ] **Step 3: 提交**

```bash
git commit docs/superpowers/plans/2026-09-16-gui-mvp-ladder.md docs/superpowers/specs/2026-09-17-layout-provenance-design.md -m "docs(gui): S8 prov-v0 落地回填——阶梯增补 + 规格里程碑重编号(S5.5→S8)"
```

---

## 计划外登记(不在本计划,防实现越界)

- **W3**:E8191/E8192 编译期静态验证 + E8193 lint——依赖 gui_lower/comptime 检查面(第二波 W1–W3),独立计划;
- **M2**:浮层原语公共化(`floating` 全量映射)+ inspector 停靠/浮动双模式——依赖包级 use 集成(P2-B)与 dev 白名单口径,独立计划;
- std/gui 升格:prov 模块随 M2 从夹具升入 `std/gui/`(夹具内嵌快照口径随 P2-B 收敛)。
