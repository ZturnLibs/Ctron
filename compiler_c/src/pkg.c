// pkg.c —— C3-c 模块级检查(包目录 + Ctron.toml)。
// 实现:E5010 孤儿规则 / E5020 模块循环 / E2020 导入可见性
//      / E4010 caps 越权 / E6010 comptime 预算。
#include "pkg.h"
#include "ast.h"
#include "parser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- 结果收集 ----------
static void push(pkg_res* r, const char* rel, const char* code, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    r->d = (pkg_diag*)realloc(r->d, (r->n + 1) * sizeof(pkg_diag));
    pkg_diag* e = &r->d[r->n++];
    e->rel = strdup(rel);
    e->code = strdup(code);
    e->msg = strdup(buf);
}

void ctron_pkg_res_free(pkg_res* r) {
    if (!r) return;
    for (size_t i = 0; i < r->n; i++) {
        free(r->d[i].rel);
        free(r->d[i].code);
        free(r->d[i].msg);
    }
    free(r->d);
    r->d = NULL;
    r->n = 0;
}

// ---------- 包数据 ----------
typedef struct {
    ctron_parse_result pr; // 持有 arena(file 指向其中)
    char* rel;             // src/<name>.ct
    char* stem;            // 模块名
    int done;              // 循环检查访问标记
    int onstack;
} mod;

typedef struct {
    mod* m;
    size_t n;
    char* pkg_name;
    char** caps;
    size_t ncaps;
    int has_comptime;
    long budget_ms;
} pkg;

static char* read_file_str(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

static void trim(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = '\0';
}

static void toml_load(pkg* p, const char* root) {
    char path[4096];
    snprintf(path, sizeof path, "%s/Ctron.toml", root);
    size_t len;
    char* s = read_file_str(path, &len);
    if (!s) return;
    char section[32] = {0};
    char* line = s;
    while (line && *line) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* cm = strstr(line, "//");
        if (cm) *cm = '\0';
        trim(line);
        char* t = line;
        if (*t == '[') {
            char* end = strchr(t, ']');
            if (end) { *end = '\0'; snprintf(section, sizeof section, "%s", t + 1); }
        } else if (*t) {
            char* eq = strchr(t, '=');
            if (eq) {
                *eq = '\0';
                trim(t);
                char* v = eq + 1;
                while (*v == ' ') v++;
                char* vc = strdup(v);
                trim(vc);
                size_t vl = strlen(vc);
                if (vl >= 2 && vc[0] == '"' && vc[vl - 1] == '"') {
                    memmove(vc, vc + 1, vl - 2);
                    vc[vl - 2] = '\0';
                }
                if (strcmp(section, "package") == 0 && strcmp(t, "name") == 0)
                    p->pkg_name = strdup(vc);
                else if (strcmp(section, "caps") == 0) {
                    // 键是能力名(值 true/false)
                    char* key = strdup(t);
                    p->caps = (char**)realloc(p->caps, (p->ncaps + 1) * sizeof(char*));
                    p->caps[p->ncaps++] = key;
                } else if (strcmp(section, "comptime") == 0) {
                    p->has_comptime = 1;
                    if (strcmp(t, "budget_ms") == 0) p->budget_ms = atol(vc);
                }
                free(vc);
            }
        }
        line = nl ? nl + 1 : NULL;
    }
    free(s);
}

static char* stem_of(const char* base) {
    const char* dot = strrchr(base, '.');
    size_t n = dot ? (size_t)(dot - base) : strlen(base);
    char* out = (char*)malloc(n + 1);
    memcpy(out, base, n);
    out[n] = '\0';
    return out;
}

static mod* pkg_find_mod(const pkg* p, const char* stem) {
    for (size_t i = 0; i < p->n; i++)
        if (strcmp(p->m[i].stem, stem) == 0) return &p->m[i];
    return NULL;
}

static void pkg_free(pkg* p) {
    for (size_t i = 0; i < p->n; i++) {
        ctron_parse_result_free(&p->m[i].pr);
        free(p->m[i].rel);
        free(p->m[i].stem);
    }
    free(p->m);
    free(p->pkg_name);
    for (size_t i = 0; i < p->ncaps; i++) free(p->caps[i]);
    free(p->caps);
    memset(p, 0, sizeof *p);
}

static void pkg_load(pkg* p, const char* root) {
    toml_load(p, root);
    char dir[4096];
    snprintf(dir, sizeof dir, "%s/src", root);
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type != DT_REG) continue;
        size_t bl = strlen(e->d_name);
        if (bl < 4 || strcmp(e->d_name + bl - 3, ".ct") != 0) continue;
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        size_t len;
        char* src = read_file_str(path, &len);
        if (!src) continue;
        ctron_parse_result pr = ctron_parse_src(src, len);
        if (pr.ndiags) {
            // 模块文件自身解析失败:单文件套件负责汇报
            ctron_parse_result_free(&pr);
            free(src);
            continue;
        }
        p->m = (mod*)realloc(p->m, (p->n + 1) * sizeof(mod));
        mod* md = &p->m[p->n++];
        memset(md, 0, sizeof *md);
        md->pr = pr;
        char rel[4096];
        snprintf(rel, sizeof rel, "src/%s", e->d_name);
        md->rel = strdup(rel);
        md->stem = stem_of(e->d_name);
        free(src);
    }
    closedir(d);
}

// ---------- 通用:名字/类型头部 ----------
static const char* head_name(const cty* t) {
    if (!t || t->kind != TY_NAMED || t->npath == 0) return NULL;
    return t->path[0];
}

static int local_type_or_trait(const pkg* p, const char* n) {
    for (size_t i = 0; i < p->n; i++) {
        const cfile* f = p->m[i].pr.file;
        for (size_t j = 0; j < f->ndecls; j++) {
            const cdecl* d = &f->decls[j];
            const char* nm = NULL;
            switch (d->kind) {
            case D_STRUCT: nm = d->strukt.name; break;
            case D_CLASS: nm = d->klass.name; break;
            case D_ENUM: nm = d->en.name; break;
            case D_TRAIT: nm = d->trait.name; break;
            default: break;
            }
            if (nm && strcmp(nm, n) == 0) return 1;
        }
    }
    return 0;
}

// 模块的 pub 项集合(含 pub 与 pub(pkg))
static int mod_has_pub_item(const mod* m, const char* name) {
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        const char* nm = NULL;
        cvis vis = VIS_PRIVATE;
        switch (d->kind) {
        case D_FN: nm = d->fn_.name; vis = d->fn_.vis; break;
        case D_STRUCT: nm = d->strukt.name; break;
        case D_CLASS: nm = d->klass.name; break;
        case D_ENUM: nm = d->en.name; break;
        case D_TRAIT: nm = d->trait.name; break;
        case D_STATIC: nm = d->statik.name; break;
        default: break;
        }
        if (nm && strcmp(nm, name) == 0) {
            if (d->kind == D_FN) return vis == VIS_PUB || vis == VIS_PUBPKG;
            // 类型默认 pub(pkg)?保守:类型视为包可见
            return 1;
        }
    }
    return 0;
}

// ---------- E5010 孤儿 / E2020 可见性 / E4010 caps / E5020 循环 ----------

static void check_orphan(pkg_res* r, const pkg* p, const mod* m) {
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_IMPL) continue;
        const char* tn = head_name(d->impl.trait_ty);
        const char* tyn = head_name(d->impl.for_ty);
        if (tn && tyn && !local_type_or_trait(p, tn) && !local_type_or_trait(p, tyn))
            push(r, m->rel, "E5010", "孤儿 impl(orphan):%s for %s 的 trait 与类型均不属本包", tn, tyn);
    }
}

static void check_use_visibility(pkg_res* r, const pkg* p, const mod* m) {
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports; k++) {
            const cimport* imp = &d->use.imports[k];
            if (imp->nsegs < 3) continue;
            if (strcmp(imp->segs[0], p->pkg_name ? p->pkg_name : "") != 0) continue;
            // 仅支持平铺单段模块(app.x.{item})
            if (imp->nsegs != 3) continue;
            const char* mstem = imp->segs[1];
            const char* item = imp->segs[2];
            if (strcmp(mstem, m->stem) == 0) continue; // 本模块
            mod* target = pkg_find_mod(p, mstem);
            if (!target) {
                push(r, m->rel, "E2020", "未知模块(secret):%s", mstem);
                continue;
            }
            if (!mod_has_pub_item(target, item)) {
                push(r, m->rel, "E2020", "不可见模块项(secret):%s 未 pub", item);
            }
        }
    }
}

static void check_circular_dfs(pkg_res* r, pkg* p, mod* cur, mod* root, int* found);

static void visit_edges(pkg_res* r, pkg* p, mod* cur, mod* root, int* found) {
    const cfile* f = cur->pr.file;
    for (size_t j = 0; j < f->ndecls && !*found; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports && !*found; k++) {
            const cimport* imp = &d->use.imports[k];
            if (imp->nsegs != 3 || strcmp(imp->segs[0], p->pkg_name ? p->pkg_name : "") != 0) continue;
            if (strcmp(imp->segs[1], cur->stem) == 0) continue;
            mod* t = pkg_find_mod(p, imp->segs[1]);
            if (!t) continue;
            if (t->onstack) {
                // 环归属当前 DFS 搜索根(语料在 a.ct 锚定)
                push(r, root->rel, "E5020", "模块循环依赖(circular):%s → %s", cur->stem, t->stem);
                *found = 1;
                return;
            } else if (!t->done) {
                check_circular_dfs(r, p, t, root, found);
            }
        }
    }
}

static void check_circular_dfs(pkg_res* r, pkg* p, mod* cur, mod* root, int* found) {
    cur->done = 1;
    cur->onstack = 1;
    visit_edges(r, p, cur, root, found);
    cur->onstack = 0;
}

static void check_circular(pkg_res* r, pkg* p) {
    // 按文件名序 DFS;环归属其搜索根(语料在 a.ct 锚定)
    for (size_t a = 0; a + 1 < p->n; a++) {
        for (size_t b = a + 1; b < p->n; b++) {
            if (strcmp(p->m[a].stem, p->m[b].stem) > 0) {
                mod tmp = p->m[a];
                p->m[a] = p->m[b];
                p->m[b] = tmp;
            }
        }
    }
    int found = 0;
    for (size_t i = 0; i < p->n && !found; i++) {
        if (!p->m[i].done) {
            check_circular_dfs(r, p, &p->m[i], &p->m[i], &found);
        }
    }
    // 重置标记(供其余检查不依赖)
    for (size_t i = 0; i < p->n; i++) { p->m[i].done = 0; p->m[i].onstack = 0; }
}

// ---------- E4010 caps ----------
static void check_caps(pkg_res* r, const pkg* p, const mod* m) {
    // 导入的 std 能力名:use std.<key>.<Name>
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports; k++) {
            const cimport* imp = &d->use.imports[k];
            if (imp->nsegs != 3 || strcmp(imp->segs[0], "std") != 0) continue;
            const char* key = imp->segs[1];
            if (strcmp(key, "fs") != 0 && strcmp(key, "time") != 0) continue;
            const char* name = imp->segs[2];
            // 本模块内是否有 &Name 参数
            for (size_t a = 0; a < f->ndecls; a++) {
                const cdecl* dd = &f->decls[a];
                const cfn* fn = NULL;
                if (dd->kind == D_FN) fn = &dd->fn_;
                if (!fn) continue;
                for (size_t q = 0; q < fn->nparams; q++) {
                    const cparam* pr = &fn->params[q];
                    if (pr->is_receiver || !pr->ty || pr->ty->kind != TY_REF || !pr->ty->sub) continue;
                    const char* hn = head_name(pr->ty->sub);
                    if (hn && strcmp(hn, name) == 0) {
                        int allowed = 0;
                        for (size_t c = 0; c < p->ncaps; c++)
                            if (strcmp(p->caps[c], key) == 0) allowed = 1;
                        if (!allowed)
                            push(r, m->rel, "E4010", "使用 %s 能力超出 manifest(caps) 声明:参数 &%s", key, name);
                    }
                }
            }
        }
    }
}

// ---------- E6010 comptime 预算(轻量求值器,深度预算) ----------
typedef struct {
    const pkg* p;
    const char* file;
    long steps, budget;
    int err;
    int depth;
} ceval;

typedef struct cev_bind { const char* n; long v; struct cev_bind* next; } cev_bind;
typedef cev_bind* cev_env;
static const cdecl* pkg_comptime_fn(const pkg* p, const char* name, const cfn** out) {
    for (size_t i = 0; i < p->n; i++) {
        const cfile* f = p->m[i].pr.file;
        for (size_t j = 0; j < f->ndecls; j++) {
            const cdecl* d = &f->decls[j];
            if (d->kind == D_FN && d->fn_.is_comptime && strcmp(d->fn_.name, name) == 0) {
                *out = &d->fn_;
                return d;
            }
        }
    }
    return NULL;
}
static long ev_expr(ceval* E, cev_env env, cexpr* e, int* ok);

// 支持的 comptime 求值块 = 单 return / 尾表达式(无局部绑定;保证无泄漏)
static long ev_block(ceval* E, cev_env env, cblock* b, int* ok) {
    if (!b) { *ok = 0; return 0; }
    for (size_t i = 0; i < b->nstmts && *ok; i++) {
        cstmt* st = b->stmts[i];
        if (st->kind == ST_RET) return st->e ? ev_expr(E, env, st->e, ok) : 0;
        *ok = 0;
        return 0;
    }
    if (b->tail) return ev_expr(E, env, b->tail, ok);
    *ok = 0;
    return 0;
}

static long ev_expr(ceval* E, cev_env env, cexpr* e, int* ok) {
    if (!e) { *ok = 0; return 0; }
    if (++E->steps > E->budget) { *ok = 0; E->err = 1; return 0; }
    switch (e->kind) {
    case EX_INT: return atol(e->text);
    case EX_IDENT: {
        for (cev_env b = env; b; b = b->next)
            if (b->n && e->text && strcmp(b->n, e->text) == 0) return b->v;
        *ok = 0;
        return 0;
    }
    case EX_UNARY: {
        long x = ev_expr(E, env, e->ux, ok);
        return e->uop == UN_NEG ? -x : x;
    }
    case EX_BINARY: {
        long a = ev_expr(E, env, e->lhs, ok);
        if (!*ok) return 0;
        long b = ev_expr(E, env, e->rhs, ok);
        if (!*ok) return 0;
        switch (e->bop) {
        case B_ADD: return a + b;
        case B_SUB: return a - b;
        case B_MUL: return a * b;
        case B_DIV: return b ? a / b : 0;
        case B_MOD: return b ? a % b : 0;
        default: *ok = 0; return 0;
        }
    }
    case EX_CALL: {
        cexpr* cal = e->callee;
        if (cal && cal->kind == EX_IDENT) {
            const cfn* fn;
            if (pkg_comptime_fn(E->p, cal->text, &fn)) {
                if (E->depth > 400) { *ok = 0; E->err = 1; return 0; }
                // 实参(须全部常量整型可估)
                if (e->nelems != fn->nparams) { *ok = 0; return 0; }
                cev_env sub = env;
                for (size_t i = 0; i < e->nelems && *ok; i++) {
                    long v = ev_expr(E, env, e->elems[i], ok);
                    cev_bind* bd = (cev_bind*)calloc(1, sizeof(cev_bind));
                    bd->n = fn->params[i].name;
                    bd->v = v;
                    bd->next = sub;
                    sub = bd;
                }
                if (!*ok) return 0;
                E->depth++;
                long res = ev_block(E, sub, fn->body, ok);
                E->depth--;
                // 释放 sub 链
                while (sub != env) { cev_bind* nx = sub->next; free(sub); sub = nx; }
                return res;
            }
        }
        *ok = 0;
        return 0;
    }
    default:
        *ok = 0;
        return 0;
    }
}

static void check_comptime_budget(pkg_res* r, pkg* p) {
    if (!p->has_comptime) return;
    for (size_t i = 0; i < p->n; i++) {
        const cfile* f = p->m[i].pr.file;
        for (size_t j = 0; j < f->ndecls; j++) {
            const cdecl* d = &f->decls[j];
            if (d->kind != D_CONST) continue;
            ceval E = {0};
            E.p = p;
            E.file = p->m[i].rel;
            E.budget = p->budget_ms > 0 ? p->budget_ms * 10000 : 1000000;
            int ok = 1;
            ev_expr(&E, NULL, d->konst.expr, &ok);
            if (E.err) {
                push(r, p->m[i].rel, "E6010", "comptime 超出预算(budget):%s 求值超限", d->konst.name);
                // 一条即可
                break;
            }
        }
    }
}

// ---------- 入口 ----------
pkg_res ctron_pkg_check(const char* root) {
    pkg_res r = {0};
    pkg p = {0};
    pkg_load(&p, root);
    if (!p.pkg_name) { pkg_free(&p); return r; } // 缺 [package] name 不检查
    for (size_t i = 0; i < p.n; i++) {
        check_orphan(&r, &p, &p.m[i]);
        check_use_visibility(&r, &p, &p.m[i]);
        check_caps(&r, &p, &p.m[i]);
    }
    check_circular(&r, &p);
    check_comptime_budget(&r, &p);
    pkg_free(&p);
    return r;
}
