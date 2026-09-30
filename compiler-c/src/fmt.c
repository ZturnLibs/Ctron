// fmt.c —— ctron fmt(R-P2d 移植):token 流重排,不动 AST。
// 布局权威 = 原始源码间隙(docs/fmt-spec.md R1):
//   free ≥ 1 换行、free ≥ 2 换行 + 空行(至多 1 行);块多行性 = 匹配大括号间原始间隙含换行;
//   链断行 = Dot 前自由换行 → 换行 + 相对缩进 1 级;`} else` 恒同行;
//   注释从 token span 间隙回收逐字回贴;字面量按 span 源切片零改写。
// 与 compiler-rust/src/fmt.rs 逐条对齐;三宿主输出逐字节一致由 tests/fmt/parity.sh 把关。
#include <stdlib.h>
#include <string.h>

#include "fmt.h"
#include "lexer.h"

#define NO_PAIR ((size_t)-1)

typedef struct {
    size_t start;
    size_t end; // 不含行尾换行;换行留给统一的间隙计数
} fmt_comment;

static void* xmalloc(size_t n) {
    void* p = malloc(n ? n : 1);
    if (!p) abort();
    return p;
}

static void* xrealloc(void* p, size_t n) {
    void* q = realloc(p, n ? n : 1);
    if (!q) abort();
    return q;
}

// ---------- 输出缓冲 ----------
typedef struct {
    char* buf;
    size_t len, cap;
} obuf;

static void ob_putn(obuf* o, const char* s, size_t n) {
    if (o->len + n + 1 > o->cap) {
        while (o->len + n + 1 > o->cap) o->cap = o->cap ? o->cap * 2 : 256;
        o->buf = (char*)xrealloc(o->buf, o->cap);
    }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
    o->buf[o->len] = '\0';
}

static void ob_putc(obuf* o, char c) { ob_putn(o, &c, 1); }

static const char OB_EMPTY[1] = "";

// ---------- 记号文本(规范拼写的权威面;与 fmt.rs tok_text 对齐) ----------
static const char* tok_text(ctron_tok_kind k) {
    switch (k) {
    case TOK_FN: return "fn";
    case TOK_LET: return "let";
    case TOK_VAR: return "var";
    case TOK_CONST: return "const";
    case TOK_STATIC: return "static";
    case TOK_COMPTIME: return "comptime";
    case TOK_IF: return "if";
    case TOK_ELSE: return "else";
    case TOK_MATCH: return "match";
    case TOK_WHILE: return "while";
    case TOK_FOR: return "for";
    case TOK_IN: return "in";
    case TOK_BREAK: return "break";
    case TOK_CONTINUE: return "continue";
    case TOK_RETURN: return "return";
    case TOK_STRUCT: return "struct";
    case TOK_CLASS: return "class";
    case TOK_ENUM: return "enum";
    case TOK_TRAIT: return "trait";
    case TOK_IMPL: return "impl";
    case TOK_OWN: return "own";
    case TOK_SCOPE: return "scope";
    case TOK_TEST: return "test";
    case TOK_USE: return "use";
    case TOK_PUB: return "pub";
    case TOK_EXTERN: return "extern";
    case TOK_PROP: return "prop";
    case TOK_TRUE: return "true";
    case TOK_FALSE: return "false";
    case TOK_VOID: return "void";
    case TOK_SELF: return "self";
    case TOK_OR: return "or";
    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_STAR: return "*";
    case TOK_SLASH: return "/";
    case TOK_PERCENT: return "%";
    case TOK_WRAP_PLUS: return "+%";
    case TOK_WRAP_MINUS: return "-%";
    case TOK_PLUS_EQ: return "+=";
    case TOK_MINUS_EQ: return "-=";
    case TOK_STAR_EQ: return "*=";
    case TOK_SLASH_EQ: return "/=";
    case TOK_PERCENT_EQ: return "%=";
    case TOK_EQ_EQ: return "==";
    case TOK_NOT_EQ: return "!=";
    case TOK_LT: return "<";
    case TOK_GT: return ">";
    case TOK_LT_EQ: return "<=";
    case TOK_GT_EQ: return ">=";
    case TOK_ASSIGN: return "=";
    case TOK_AND_AND: return "&&";
    case TOK_OR_OR: return "||";
    case TOK_DOT_DOT: return "..";
    case TOK_DOT_DOT_EQ: return "..=";
    case TOK_ELLIPSIS: return "...";
    case TOK_ARROW: return "->";
    case TOK_FAT_ARROW: return "=>";
    case TOK_QUESTION: return "?";
    case TOK_DOT: return ".";
    case TOK_COMMA: return ",";
    case TOK_COLON: return ":";
    case TOK_LBRACKET: return "[";
    case TOK_RBRACKET: return "]";
    case TOK_LPAREN: return "(";
    case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "{";
    case TOK_RBRACE: return "}";
    case TOK_PIPE: return "|";
    case TOK_AMP: return "&";
    case TOK_HASH: return "#";
    case TOK_AT: return "@";
    case TOK_UNDERSCORE: return "_";
    case TOK_BANG: return "!";
    default: return "";
    }
}

static int operand_end(ctron_tok_kind k) {
    switch (k) {
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_STR:
    case TOK_IDENT:
    case TOK_RPAREN:
    case TOK_RBRACKET:
    case TOK_QUESTION:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_UNDERSCORE:
    case TOK_RBRACE:
    case TOK_SELF: return 1;
    default: return 0;
    }
}

// ---------- 发射器状态 ----------
typedef struct {
    const char* src;
    size_t len;
    ctron_token* toks;
    size_t nt;
    fmt_comment* comments;
    size_t nc, ci; // ci:已消费注释游标(按 start 升序)
    size_t* partner;
    obuf out;
    size_t indent;
    int at_line_start;
    size_t extra; // 行内相对缩进(链断行),换行时清零
    size_t last_end;
    ctron_tok_kind prev;
    int has_prev;
    int pipe_open;
    int sign_unary;
} F;

static void wr(F* f, const char* s, size_t n) {
    if (f->at_line_start) {
        size_t ind = (f->indent + f->extra) * 4;
        for (size_t i = 0; i < ind; i++) ob_putc(&f->out, ' ');
        f->at_line_start = 0;
    }
    ob_putn(&f->out, s, n);
}

static void wr_sep_n(F* f, const char* s, size_t n, int space_before) {
    if (!f->at_line_start && space_before) ob_putc(&f->out, ' ');
    wr(f, s, n);
}

static void wr_sep(F* f, const char* s, int space_before) {
    wr_sep_n(f, s, strlen(s), space_before);
}

static void emit_newline(F* f, int blank) {
    ob_putc(&f->out, '\n');
    if (blank) ob_putc(&f->out, '\n');
    f->at_line_start = 1;
    f->extra = 0;
}

// [from, to) 内自由换行数(扣除注释占据的换行;注释 end 不含行尾换行)
static size_t free_newlines(F* f, size_t from, size_t to) {
    if (to <= from) return 0;
    if (to > f->len) to = f->len;
    size_t count = 0;
    for (size_t i = from; i < to; i++) {
        if (f->src[i] != '\n') continue;
        int covered = 0;
        for (size_t c = f->ci; c < f->nc; c++) {
            if (f->comments[c].start > i) break;
            if (i < f->comments[c].end) { covered = 1; break; }
        }
        if (!covered) count++;
    }
    return count;
}

static int block_multiline(F* f, size_t open_idx) {
    size_t close = f->partner[open_idx];
    if (close == NO_PAIR) return 0;
    size_t s = f->toks[open_idx].span.end;
    size_t e = f->toks[close].span.start;
    if (e > f->len) e = f->len;
    for (size_t i = s; i < e; i++)
        if (f->src[i] == '\n') return 1;
    return 0;
}

static void push_real(F* f, ctron_tok_kind k, size_t end) {
    f->prev = k;
    f->has_prev = 1;
    f->last_end = end;
}

// 空格表:是否在当前记号前补一个空格(行首缩进由 wr 处理)
static int needs_space(F* f, ctron_tok_kind cur) {
    if (!f->has_prev) return 0;
    ctron_tok_kind prev = f->prev;
    // cur 侧紧贴(含 ... 变参:Rust 侧按 Dot Dot Dot 发射,逗号后恒紧贴)
    if (cur == TOK_COMMA || cur == TOK_COLON || cur == TOK_RPAREN || cur == TOK_RBRACKET ||
        cur == TOK_DOT || cur == TOK_QUESTION || cur == TOK_ELLIPSIS) return 0;
    if (cur == TOK_RBRACE && prev == TOK_LBRACE) return 0; // 空块 {}
    if (cur == TOK_LBRACKET && (prev == TOK_IDENT || prev == TOK_RPAREN || prev == TOK_RBRACKET ||
                                prev == TOK_QUESTION || prev == TOK_HASH)) return 0;
    if (cur == TOK_LPAREN && (prev == TOK_IDENT || prev == TOK_RPAREN || prev == TOK_RBRACKET ||
                              prev == TOK_QUESTION || prev == TOK_SELF)) return 0;
    // 前缀 ! & 的紧贴只在其右侧(prev 侧);左侧恒空格,防 `return!(...)`/`&&!b`
    // prev 侧紧贴
    if (prev == TOK_LPAREN || prev == TOK_LBRACKET || prev == TOK_DOT || prev == TOK_AT ||
        prev == TOK_HASH || prev == TOK_QUESTION) return 0;
    if (prev == TOK_BANG || prev == TOK_AMP) return 0;
    if ((prev == TOK_MINUS || prev == TOK_PLUS) && f->sign_unary) return 0;
    if (prev == TOK_PIPE && f->pipe_open) return 0; // 闭包参数起点
    // range 无空格
    if (prev == TOK_DOT_DOT || prev == TOK_DOT_DOT_EQ || cur == TOK_DOT_DOT ||
        cur == TOK_DOT_DOT_EQ) return 0;
    return 1;
}

// 统一间隙换行:free ≥ 1 换行,free ≥ 2 加空行(至多一行);
// 行首状态(前一内容已自终止)时只补空行差额
static void apply_gap_break(F* f, size_t cur_start) {
    size_t free = free_newlines(f, f->last_end, cur_start);
    if (free >= 1) {
        if (!f->at_line_start) {
            emit_newline(f, free >= 2);
        } else if (free >= 2) {
            ob_putc(&f->out, '\n');
        }
    }
}

// 发射位于 `before` 之前的待处理注释:
// 前置自由换行先落(含空行折叠);独占行注释自终止(落行尾换行),行尾注释挂当前行
static void emit_comments_before(F* f, size_t before) {
    while (f->ci < f->nc && f->comments[f->ci].start < before) {
        fmt_comment c = f->comments[f->ci];
        f->ci++;
        size_t te = c.end;
        while (te > c.start && (f->src[te - 1] == ' ' || f->src[te - 1] == '\t' ||
                                f->src[te - 1] == '\r')) te--;
        size_t pre_free = free_newlines(f, f->last_end, c.start);
        if (pre_free >= 1) {
            // 独占行注释(前置有换行)
            if (!f->at_line_start) {
                emit_newline(f, pre_free >= 2);
            } else if (pre_free >= 2) {
                ob_putc(&f->out, '\n');
            }
            wr(f, f->src + c.start, te - c.start);
            ob_putc(&f->out, '\n');
            f->at_line_start = 1;
        } else {
            // 行尾注释:挂当前行
            if (!f->at_line_start) ob_putc(&f->out, ' ');
            wr(f, f->src + c.start, te - c.start);
        }
        f->last_end = c.end;
    }
}

static void run(F* f) {
    for (size_t i = 0; i < f->nt; i++) {
        ctron_token* t = &f->toks[i];
        ctron_tok_kind k = t->kind;
        if (k == TOK_NEWLINE) continue; // 过滤流中的换行不是布局权威(原始间隙统一处理)
        if (k == TOK_EOF) {
            emit_comments_before(f, t->span.start); // Eof 只负责尾部注释
            continue;
        }
        emit_comments_before(f, t->span.start);
        if (k != TOK_ELSE) apply_gap_break(f, t->span.start);
        switch (k) {
        case TOK_LBRACE: {
            int ml = block_multiline(f, i);
            int space = needs_space(f, k);
            wr_sep(f, "{", space);
            if (ml) {
                f->indent++;
                emit_newline(f, 0);
            }
            push_real(f, k, t->span.end);
            break;
        }
        case TOK_RBRACE: {
            int ml = f->partner[i] != NO_PAIR ? block_multiline(f, f->partner[i]) : 0;
            // 前隙换行已由 apply_gap_break 统一落下;此处只负责降缩进
            if (ml && f->indent > 0) f->indent--;
            int space = !ml && needs_space(f, k);
            wr_sep(f, "}", space);
            push_real(f, k, t->span.end);
            break;
        }
        case TOK_ELSE:
            // `} else` 权威同行:前隙换行一律吞掉
            wr_sep(f, "else", 1);
            push_real(f, k, t->span.end);
            break;
        case TOK_PIPE:
            if (f->pipe_open) {
                wr_sep(f, "|", 0); // 闭包参数收尾紧贴
                f->pipe_open = 0;
            } else {
                int tight = f->has_prev &&
                            (f->prev == TOK_COMMA || f->prev == TOK_LPAREN ||
                             f->prev == TOK_IDENT || f->prev == TOK_RPAREN ||
                             f->prev == TOK_LBRACKET);
                wr_sep(f, "|", !tight);
                f->pipe_open = 1;
            }
            push_real(f, k, t->span.end);
            break;
        case TOK_DOT: {
            // 链断行(R4 v1 定版):自由换行 → 换行 + 相对缩进 1 级,空行折叠沿用 R1
            size_t free = free_newlines(f, f->last_end, t->span.start);
            if (free >= 1) {
                if (!f->at_line_start) {
                    emit_newline(f, free >= 2);
                } else if (free >= 2) {
                    ob_putc(&f->out, '\n');
                }
                f->extra = 1;
            }
            wr_sep(f, ".", 0);
            push_real(f, k, t->span.end);
            break;
        }
        case TOK_MINUS:
        case TOK_PLUS:
            f->sign_unary = !f->has_prev || !operand_end(f->prev);
            wr_sep(f, tok_text(k), needs_space(f, k));
            push_real(f, k, t->span.end);
            break;
        default: {
            const char* text;
            size_t tn;
            if (k == TOK_INT || k == TOK_FLOAT || k == TOK_STR) {
                text = f->src + t->span.start; // 字面量按 span 源切片(R7)
                tn = t->span.end - t->span.start;
            } else if (k == TOK_IDENT) {
                text = t->text;
                tn = strlen(t->text);
            } else {
                text = tok_text(k);
                tn = strlen(text);
            }
            wr_sep_n(f, text, tn, needs_space(f, k));
            push_real(f, k, t->span.end);
            break;
        }
        }
    }
}

// 注释回收:token span 间隙内逐字节找 `//`,到行尾(或间隙尾)止
static fmt_comment* extract_comments(const char* src, size_t len, ctron_token* toks, size_t nt,
                                     size_t* out_n) {
    fmt_comment* out = NULL;
    size_t n = 0, cap = 0;
    size_t prev_end = 0;
    size_t gi = 0;
    for (;;) {
        size_t gs, ge;
        if (gi < nt) {
            if (toks[gi].span.start > prev_end) {
                gs = prev_end;
                ge = toks[gi].span.start;
            } else {
                gs = ge = 0;
            }
            if (toks[gi].span.end > prev_end) prev_end = toks[gi].span.end;
            gi++;
        } else if (prev_end < len) {
            gs = prev_end;
            ge = len;
            prev_end = len;
        } else {
            break;
        }
        if (ge <= gs) continue;
        for (size_t i = gs; i + 1 < ge; i++) {
            if (src[i] == '/' && src[i + 1] == '/') {
                size_t j = i + 2;
                while (j < ge && src[j] != '\n') j++;
                if (n == cap) {
                    cap = cap ? cap * 2 : 16;
                    out = (fmt_comment*)xrealloc(out, cap * sizeof(fmt_comment));
                }
                out[n].start = i;
                out[n].end = j;
                n++;
                i = j > i + 2 ? j - 1 : i + 1; // for 会 ++;跳过注释体
            }
        }
        if (gi >= nt && prev_end >= len) break;
    }
    *out_n = n;
    return out;
}

static size_t* match_braces(ctron_token* toks, size_t nt) {
    size_t* partner = (size_t*)xmalloc(nt * sizeof(size_t));
    for (size_t i = 0; i < nt; i++) partner[i] = NO_PAIR;
    size_t* stack = (size_t*)xmalloc(nt * sizeof(size_t));
    size_t top = 0;
    for (size_t i = 0; i < nt; i++) {
        if (toks[i].kind == TOK_LBRACE) {
            stack[top++] = i;
        } else if (toks[i].kind == TOK_RBRACE) {
            if (top > 0) {
                size_t open = stack[--top];
                partner[open] = i;
                partner[i] = open;
            }
        }
    }
    free(stack);
    return partner;
}

// ---------- R9/R10 use 预处理(2026-09-30):导入排序 + 长组自动折行 ----------
// 落点 = 发射前源文本重写(仅注释受控的 use 语句 span),主发射器零改动;
// 归一化输出为定点(排序稳定 + 多行不回折),二次调用恒等,天然幂等。
// 与 compiler-rust/src/fmt.rs collect_uses/normalize_uses 逐条对齐;
// 三宿主输出逐字节一致由 tests/fmt/parity.sh 把关。规范:docs/fmt-spec.md R9/R10。

#define USE_WRAP_COLS 100

typedef struct {
    const char* key; // 导入名 = 项首标识符(借用 toks arena;别名项按原名)
    size_t s, e;     // 项文本字节范围
} u_item;

typedef struct {
    u_item* items;
    size_t ni;
    int trailing;    // 原组以尾逗号收(重建多行按原样保留)
    int single_line; // 组 {…} 之间原始无换行
    int sorted;      // 项键已非降序
} u_group;

typedef struct {
    size_t start, end; // 语句字节范围(use 记号起)
    size_t ps, pe;     // 路径字节范围
    u_group* group;    // NULL = 无组/空组
    int frozen;        // 语句 span 含注释(不排序不重排)
} u_stmt;

static int is_path_ident(ctron_token* t) {
    return t->kind == TOK_IDENT && !(t->text && strcmp(t->text, "as") == 0);
}

static int span_has_comment(fmt_comment* cs, size_t nc, size_t s, size_t e) {
    for (size_t i = 0; i < nc; i++)
        if (cs[i].start < e && cs[i].end > s) return 1;
    return 0;
}

// [from, to) 自由换行数(扣注释占据的换行);与发射器 free_newlines 同口径
static size_t use_free_nl(const char* src, fmt_comment* cs, size_t nc, size_t from, size_t to) {
    if (to <= from) return 0;
    size_t count = 0;
    for (size_t i = from; i < to; i++) {
        if (src[i] != '\n') continue;
        int covered = 0;
        for (size_t c = 0; c < nc; c++) {
            if (cs[c].start > i) break;
            if (i < cs[c].end) { covered = 1; break; }
        }
        if (!covered) count++;
    }
    return count;
}

// 收集全部 use 语句;返回 0 成功,-1 = 形态不识(整文件放弃预处理,退回纯 R1)
static int collect_uses(const char* src, ctron_token* toks, size_t nt, size_t* partner,
                        fmt_comment* comments, size_t nc, u_stmt** out, size_t* out_n) {
    (void)src;
    u_stmt* arr = NULL;
    size_t n = 0, cap = 0;
    size_t i = 0;
    while (i < nt) {
        if (toks[i].kind != TOK_USE) { i++; continue; }
        u_stmt st;
        memset(&st, 0, sizeof st);
        st.start = toks[i].span.start;
        size_t j = i + 1;
        if (j >= nt || !is_path_ident(&toks[j])) return -1;
        size_t pfirst = j, plast = j;
        j++;
        int grouped = 0;
        size_t open = 0, close = 0;
        for (;;) {
            if (j < nt && toks[j].kind == TOK_DOT) {
                if (j + 1 < nt && toks[j + 1].kind == TOK_LBRACE) { grouped = 1; break; }
                j++;
                if (j >= nt || !is_path_ident(&toks[j])) return -1;
                plast = j;
                j++;
                continue;
            }
            break;
        }
        u_group* g = NULL;
        size_t end_tok;
        if (grouped) {
            open = j + 1;
            if (open >= nt) return -1;
            close = partner[open];
            if (close == NO_PAIR || close <= open) return -1;
            g = (u_group*)xmalloc(sizeof(u_group));
            memset(g, 0, sizeof *g);
            u_item* items = NULL;
            size_t ni = 0, cap2 = 0;
            size_t cur_first = 0, cur_s = 0, cur_e = 0;
            int in_cur = 0;
            int bad = 0;
            for (size_t k = open + 1; k < close; k++) {
                ctron_token* t = &toks[k];
                if (t->kind == TOK_NEWLINE) continue;
                if (t->kind == TOK_COMMA) {
                    if (!in_cur) { bad = 1; break; } // 空项(如 `.{ a,,b }`)
                    if (ni == cap2) {
                        cap2 = cap2 ? cap2 * 2 : 8;
                        items = (u_item*)xrealloc(items, cap2 * sizeof(u_item));
                    }
                    items[ni].key = toks[cur_first].text;
                    items[ni].s = cur_s;
                    items[ni].e = cur_e;
                    ni++;
                    in_cur = 0;
                } else if (t->kind == TOK_IDENT) {
                    if (!in_cur) {
                        cur_first = k;
                        cur_s = t->span.start;
                        if (t->text && strcmp(t->text, "as") == 0) { bad = 1; break; }
                    }
                    cur_e = t->span.end;
                    in_cur = 1;
                } else {
                    bad = 1;
                    break;
                }
            }
            if (bad) { free(items); free(g); return -1; }
            if (in_cur) {
                if (ni == cap2) {
                    cap2 = cap2 ? cap2 * 2 : 8;
                    items = (u_item*)xrealloc(items, cap2 * sizeof(u_item));
                }
                items[ni].key = toks[cur_first].text;
                items[ni].s = cur_s;
                items[ni].e = cur_e;
                ni++;
                g->trailing = 0;
            } else {
                g->trailing = ni > 0;
            }
            g->items = items;
            g->ni = ni;
            if (ni == 0) { free(items); free(g); g = NULL; } // 空组无事可做
            if (g) {
                g->single_line = 1;
                for (size_t b = toks[open].span.end; b < toks[close].span.start; b++)
                    if (src[b] == '\n') { g->single_line = 0; break; }
                g->sorted = 1;
                for (size_t a = 0; a + 1 < g->ni; a++)
                    if (strcmp(g->items[a].key, g->items[a + 1].key) > 0) { g->sorted = 0; break; }
            }
            end_tok = close;
        } else {
            end_tok = plast;
        }
        // 尾随别名(`use a.b.C as D`)并入语句 span,防排序后悬空
        if (end_tok + 1 < nt && toks[end_tok + 1].kind == TOK_IDENT && toks[end_tok + 1].text &&
            strcmp(toks[end_tok + 1].text, "as") == 0) {
            if (end_tok + 2 >= nt || !is_path_ident(&toks[end_tok + 2])) return -1;
            end_tok += 2;
        }
        st.end = toks[end_tok].span.end;
        st.ps = toks[pfirst].span.start;
        st.pe = toks[plast].span.end;
        st.group = g;
        st.frozen = span_has_comment(comments, nc, st.start, st.end);
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            arr = (u_stmt*)xrealloc(arr, cap * sizeof(u_stmt));
        }
        arr[n++] = st;
        i = end_tok + 1;
    }
    *out = arr;
    *out_n = n;
    return 0;
}

static int path_cmp(const char* src, const u_stmt* a, const u_stmt* b) {
    size_t la = a->pe - a->ps, lb = b->pe - b->ps;
    size_t m = la < lb ? la : lb;
    int c = memcmp(src + a->ps, src + b->ps, m);
    if (c) return c;
    return la < lb ? -1 : la > lb ? 1 : 0;
}

// 单语句改写判定与重建(R9/R10);不改写则原样切片。重建一律用排序后的项序。
static void stmt_text_append(obuf* o, const char* src, const u_stmt* st) {
    const u_group* g = st->group;
    if (!g || st->frozen) {
        ob_putn(o, src + st->start, st->end - st->start);
        return;
    }
    size_t n = g->ni;
    size_t* ord = (size_t*)xmalloc(n * sizeof(size_t));
    for (size_t a = 0; a < n; a++) ord[a] = a;
    for (size_t a = 1; a < n; a++) { // 稳定插入排序,按 key 字节序
        size_t v = ord[a], b = a;
        while (b > 0 && strcmp(g->items[ord[b - 1]].key, g->items[v].key) > 0) {
            ord[b] = ord[b - 1];
            b--;
        }
        ord[b] = v;
    }
    // 内联规范形态(量宽用):use 路径.{ 项, 项 }
    obuf in = {0};
    ob_putn(&in, "use ", 4);
    ob_putn(&in, src + st->ps, st->pe - st->ps);
    ob_putn(&in, ".{ ", 3);
    for (size_t a = 0; a < n; a++) {
        if (a) ob_putn(&in, ", ", 2);
        const u_item* it = &g->items[ord[a]];
        ob_putn(&in, src + it->s, it->e - it->s);
    }
    ob_putn(&in, " }", 2);
    int over = g->single_line && in.len > USE_WRAP_COLS;
    if (g->sorted && !over) {
        ob_putn(o, src + st->start, st->end - st->start);
        free(in.buf);
        free(ord);
        return;
    }
    if (g->single_line) {
        if (over) { // R9 自动折行:恒补尾逗号
            ob_putn(o, "use ", 4);
            ob_putn(o, src + st->ps, st->pe - st->ps);
            ob_putn(o, ".{\n", 3);
            for (size_t a = 0; a < n; a++) {
                const u_item* it = &g->items[ord[a]];
                ob_putn(o, "    ", 4);
                ob_putn(o, src + it->s, it->e - it->s);
                ob_putc(o, ',');
                ob_putc(o, '\n');
            }
            ob_putc(o, '}');
        } else {
            ob_putn(o, in.buf, in.len); // 单行组排序:内联规范形态
        }
    } else { // 多行组重排:每行一项,尾逗号按原样
        ob_putn(o, "use ", 4);
        ob_putn(o, src + st->ps, st->pe - st->ps);
        ob_putn(o, ".{\n", 3);
        for (size_t a = 0; a < n; a++) {
            const u_item* it = &g->items[ord[a]];
            ob_putn(o, "    ", 4);
            ob_putn(o, src + it->s, it->e - it->s);
            if (g->trailing || a + 1 < n) ob_putc(o, ',');
            ob_putc(o, '\n');
        }
        ob_putc(o, '}');
    }
    free(in.buf);
    free(ord);
}

// R9/R10 入口;无 use / 形态不识时返回 NULL(源保持原样)
static char* normalize_uses(const char* src, size_t len, ctron_token* toks, size_t nt,
                            size_t* partner, fmt_comment* comments, size_t nc, size_t* out_len) {
    u_stmt* stmts = NULL;
    size_t ns = 0;
    if (collect_uses(src, toks, nt, partner, comments, nc, &stmts, &ns) != 0) return NULL;
    if (ns == 0) {
        free(stmts);
        return NULL;
    }
    obuf o = {0};
    size_t cursor = 0;
    size_t ri = 0;
    while (ri < ns) {
        // run:语句间仅单换行或同行、无注释、无空行(空行/注释即分段)
        size_t re = ri;
        while (re + 1 < ns) {
            if (span_has_comment(comments, nc, stmts[re].end, stmts[re + 1].start)) break;
            if (use_free_nl(src, comments, nc, stmts[re].end, stmts[re + 1].start) > 1) break;
            re++;
        }
        ob_putn(&o, src + cursor, stmts[ri].start - cursor);
        // 段内稳定排序(注释守卫:run 整体 span 含注释 → 原序)
        size_t cnt = re - ri + 1;
        size_t* idx = (size_t*)xmalloc(cnt * sizeof(size_t));
        for (size_t a = 0; a < cnt; a++) idx[a] = ri + a;
        if (!span_has_comment(comments, nc, stmts[ri].start, stmts[re].end)) {
            for (size_t a = 1; a < cnt; a++) {
                size_t v = idx[a], b = a;
                while (b > 0 && path_cmp(src, &stmts[idx[b - 1]], &stmts[v]) > 0) {
                    idx[b] = idx[b - 1];
                    b--;
                }
                idx[b] = v;
            }
        }
        for (size_t a = 0; a < cnt; a++) {
            if (a) ob_putc(&o, '\n');
            stmt_text_append(&o, src, &stmts[idx[a]]);
        }
        free(idx);
        cursor = stmts[re].end;
        ri = re + 1;
    }
    ob_putn(&o, src + cursor, len - cursor);
    for (size_t a = 0; a < ns; a++) {
        if (stmts[a].group) {
            free(stmts[a].group->items);
            free(stmts[a].group);
        }
    }
    free(stmts);
    *out_len = o.len;
    return o.buf;
}

static char* finish(obuf* o) {
    // 规范:无前导空行,文件以单个换行结束
    size_t s = 0, e = o->len;
    while (s < e && o->buf[s] == '\n') s++;
    while (e > s && (o->buf[e - 1] == '\n' || o->buf[e - 1] == ' ' || o->buf[e - 1] == '\t')) e--;
    if (e == s) return NULL;
    char* out = (char*)xmalloc(e - s + 2);
    memcpy(out, o->buf + s, e - s);
    out[e - s] = '\n';
    out[e - s + 1] = '\0';
    return out;
}

ctron_fmt_result ctron_fmt_src(const char* src, size_t len) {
    ctron_fmt_result r;
    r.out = NULL;
    r.diags = NULL;
    r.ndiags = 0;
    // R9/R10 预处理:归一化改变源则以新源重启(定点,二次恒等)
    char* owned = NULL;
    ctron_lex_result lr = ctron_lex(src, len);
    if (lr.ndiags > 0) {
        r.diags = (ctron_fmt_diag*)xmalloc(lr.ndiags * sizeof(ctron_fmt_diag));
        r.ndiags = lr.ndiags;
        for (size_t i = 0; i < lr.ndiags; i++) {
            r.diags[i].line = lr.diags[i].span.line;
            r.diags[i].col = lr.diags[i].span.col;
            r.diags[i].code = strdup(lr.diags[i].code ? lr.diags[i].code : "");
            r.diags[i].message = strdup(lr.diags[i].message ? lr.diags[i].message : "");
        }
        ctron_lex_result_free(&lr);
        return r;
    }
    {
        size_t nc0 = 0;
        fmt_comment* comments0 = extract_comments(src, len, lr.toks, lr.ntoks, &nc0);
        size_t* partner0 = match_braces(lr.toks, lr.ntoks);
        size_t nlen = 0;
        char* norm = normalize_uses(src, len, lr.toks, lr.ntoks, partner0, comments0, nc0, &nlen);
        free(comments0);
        free(partner0);
        if (norm) {
            ctron_lex_result_free(&lr);
            owned = norm;
            src = norm;
            len = nlen;
            lr = ctron_lex(src, len);
            if (lr.ndiags > 0) { // 结构性不可达(只重排/插换行),防御性按诊断返回
                r.diags = (ctron_fmt_diag*)xmalloc(lr.ndiags * sizeof(ctron_fmt_diag));
                r.ndiags = lr.ndiags;
                for (size_t i = 0; i < lr.ndiags; i++) {
                    r.diags[i].line = lr.diags[i].span.line;
                    r.diags[i].col = lr.diags[i].span.col;
                    r.diags[i].code = strdup(lr.diags[i].code ? lr.diags[i].code : "");
                    r.diags[i].message = strdup(lr.diags[i].message ? lr.diags[i].message : "");
                }
                ctron_lex_result_free(&lr);
                free(owned);
                return r;
            }
        }
    }
    F f;
    memset(&f, 0, sizeof f);
    f.src = src;
    f.len = len;
    f.toks = lr.toks;
    f.nt = lr.ntoks;
    f.comments = extract_comments(src, len, lr.toks, lr.ntoks, &f.nc);
    f.partner = match_braces(lr.toks, lr.ntoks);
    f.out.buf = NULL;
    f.out.len = 0;
    f.out.cap = 0;
    f.at_line_start = 1;
    f.prev = TOK_EOF;
    f.has_prev = 0;
    run(&f);
    r.out = finish(&f.out);
    if (!r.out) r.out = strdup(OB_EMPTY);
    free(f.out.buf);
    free(f.comments);
    free(f.partner);
    ctron_lex_result_free(&lr);
    free(owned);
    return r;
}

void ctron_fmt_result_free(ctron_fmt_result* r) {
    if (r->out) free(r->out);
    for (size_t i = 0; i < r->ndiags; i++) {
        free(r->diags[i].code);
        free(r->diags[i].message);
    }
    if (r->diags) free(r->diags);
    r->out = NULL;
    r->diags = NULL;
    r->ndiags = 0;
}
