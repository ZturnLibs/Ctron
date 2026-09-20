// pkg.c —— C3-c 模块级检查(包目录 + Ctron.ctcl,CTCL 清单解析见 ctcl_load)。
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
    char* pkg_version;
    char** caps;
    size_t ncaps;
    int has_comptime;
    long budget_ms;
    int budget_ok;
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

// ---------- CTCL 清单解析(v1;规范 2026-09-16 config-language-v1 §4/§5) ----------
// 作用域诊断:E5042/E5043/E5044/E5045/E5047/E5049/E5050。
// 结构性错误(E5040/E5041/E5046/E5048)本阶段静默跳读恢复,不由 C 线产出(L2 对齐)。
#define CTCL_MAXKEYS 32

static int ctcl_ident(const char* s, char* out, size_t outsz) {
    size_t i = 0;
    if (!(islower((unsigned char)s[0]) || s[0] == '_')) return 0;
    while (s[i] && (isalnum((unsigned char)s[i]) || s[i] == '_')) i++;
    if (i == 0 || i >= outsz) return 0;
    memcpy(out, s, i);
    out[i] = '\0';
    return (int)i;
}

// 字符串感知剥注释: strings 外的 // 截断;# 视为 // 并置 saw_hash(E5042 恢复口径)
static void ctcl_strip(const char* line, char* out, size_t outsz, int* saw_hash, int* unclosed) {
    size_t o = 0;
    int in_str = 0, esc = 0;
    for (const char* q = line; *q && o + 1 < outsz; q++) {
        char c = *q;
        if (in_str) {
            out[o++] = c;
            if (esc) esc = 0;
            else if (c == '\\') esc = 1;
            else if (c == '"') in_str = 0;
        } else if (c == '"') {
            in_str = 1;
            out[o++] = c;
        } else if (c == '/' && q[1] == '/') {
            break;
        } else if (c == '#') {
            *saw_hash = 1;
            break;
        } else {
            out[o++] = c;
        }
    }
    out[o] = '\0';
    *unclosed = in_str;
}

// 引号词法:解析 "…"(仅 \" 与 \\ 转义);*endp = 收尾引号之后
static int ctcl_qtoken(const char* v, char* out, size_t outsz, const char** endp) {
    if (*v != '"') return 0;
    size_t o = 0;
    const char* q = v + 1;
    while (*q && *q != '"') {
        char c = *q;
        if (c == '\\') {
            q++;
            if (*q != '"' && *q != '\\') return 0;
            c = *q;
        }
        if (o + 1 < outsz) out[o++] = c;
        q++;
    }
    if (*q != '"') return 0;
    out[o] = '\0';
    *endp = q + 1;
    return 1;
}

// 整值位置的字符串(必须整体为一对引号)
static int ctcl_str(const char* v, char* out, size_t outsz) {
    const char* endp = NULL;
    if (!ctcl_qtoken(v, out, outsz, &endp)) return 0;
    while (*endp == ' ' || *endp == '\t') endp++;
    return *endp == '\0';
}

// 整数字面量:-?(0|[1-9][0-9]*)
static int ctcl_is_int(const char* v) {
    size_t i = 0;
    if (v[0] == '-') i = 1;
    if (v[i] == '\0') return 0;
    if (v[i] == '0') return v[i + 1] == '\0';
    if (!isdigit((unsigned char)v[i])) return 0;
    for (; v[i]; i++)
        if (!isdigit((unsigned char)v[i])) return 0;
    return 1;
}

// 浮点前缀:Python re `-?[0-9]*\.[0-9]`
static int ctcl_is_float(const char* v) {
    size_t i = 0;
    if (v[0] == '-') i = 1;
    while (isdigit((unsigned char)v[i])) i++;
    if (v[i] != '.' || !isdigit((unsigned char)v[i + 1])) return 0;
    return 1;
}

// Levenshtein 距离(键名短串,≤40 截断);规则与 Python/Rust 线一致:≤2 才建议
static int ctcl_lev(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la > 40) la = 40;
    if (lb > 40) lb = 40;
    int prev[41], cur[41];
    for (size_t j = 0; j <= lb; j++) prev[j] = (int)j;
    for (size_t i = 1; i <= la; i++) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= lb; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int m = prev[j] + 1;
            if (cur[j - 1] + 1 < m) m = cur[j - 1] + 1;
            if (prev[j - 1] + cost < m) m = prev[j - 1] + cost;
            cur[j] = m;
        }
        memcpy(prev, cur, sizeof(int) * (lb + 1));
    }
    return prev[lb];
}

static int ctcl_close(const char* k, const char** keys, size_t n, char* out, size_t outsz) {
    int bestd = 99;
    const char* best = NULL;
    for (size_t i = 0; i < n; i++) {
        int d = ctcl_lev(k, keys[i]);
        if (d < bestd) {
            bestd = d;
            best = keys[i];
        }
    }
    if (best && bestd <= 2) {
        snprintf(out, outsz, "%s", best);
        return 1;
    }
    return 0;
}

static const char* PKG_KEYS[] = {"manifest_version", "name", "version", "caps"};
static const char* COMPTIME_KEYS[] = {"budget_ms"};
static const char* DEP_KEYS[] = {"path", "git", "rev", "version"};

static const char* NAME_PAT = "[a-z][a-z0-9_-]*";
static const char* SEMVER_PAT =
    "(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?(\\+[0-9A-Za-z.-]+)?";

static int c_is_name(const char* v) {
    if (!v[0] || !islower((unsigned char)v[0])) return 0;
    for (size_t i = 1; v[i]; i++)
        if (!(islower((unsigned char)v[i]) || isdigit((unsigned char)v[i]) || v[i] == '_' || v[i] == '-'))
            return 0;
    return 1;
}

static int c_is_semver(const char* v) {
    size_t i = 0;
    for (int comp = 0; comp < 3; comp++) {
        if (comp > 0) {
            if (v[i] != '.') return 0;
            i++;
        }
        if (v[i] == '0') {
            i++;
        } else if (v[i] >= '1' && v[i] <= '9') {
            i++;
            while (isdigit((unsigned char)v[i])) i++;
        } else {
            return 0;
        }
    }
    if (v[i] == '-') {
        i++;
        if (!v[i]) return 0;
        while (isalnum((unsigned char)v[i]) || v[i] == '-' || v[i] == '.') i++;
    }
    if (v[i] == '+') {
        i++;
        if (!v[i]) return 0;
        while (isalnum((unsigned char)v[i]) || v[i] == '-' || v[i] == '.') i++;
    }
    return v[i] == '\0';
}

static void ctcl_load(pkg* p, const char* path, pkg_res* r) {
    size_t len;
    char* s = read_file_str(path, &len);
    if (!s) return;
    char raw[4096], line[4096];
    char block[32] = {0};        // "" = 块外;"skip" = 错误构造跳读
    char seen_keys[CTCL_MAXKEYS][32];
    char seen_deps[CTCL_MAXKEYS][64];
    int seen_dep_ln[CTCL_MAXKEYS];
    int nseen = 0, ndeps = 0;
    int saw_pkg = 0, saw_name = 0, saw_version = 0, has_mver = 0, mver_ok = 0;
    int dep_active = 0, has_path = 0, has_git = 0, has_rev = 0, has_version = 0;
    int skipping = 0, bal = 0;
    pkg_res pend = {0};
    int b_saw_pkg = 0, b_has_mver = 0, b_mver_ok = 0, b_saw_name = 0, b_saw_version = 0;
    int b_comptime_seen = 0, b_budget_ok = 0;
    long b_budget = 0;

    char* cursor = s;
    int ln_num = 0;
    while (*cursor) {
        ln_num++;
        char* nl = strchr(cursor, '\n');
        size_t ll = nl ? (size_t)(nl - cursor) : strlen(cursor);
        size_t cp = ll < sizeof raw - 1 ? ll : sizeof raw - 1;
        memcpy(raw, cursor, cp);
        raw[cp] = '\0';
        cursor = nl ? nl + 1 : cursor + ll;

        int saw_hash = 0;
        int unclosed = 0;
        ctcl_strip(raw, line, sizeof line, &saw_hash, &unclosed);
        if (unclosed) {
            push(r, "Ctron.ctcl", "E5040", "字符串未闭合");
            continue;
        }
        if (saw_hash)
            push(r, "Ctron.ctcl", "E5042", "本语言注释是 // 而非 #(与宿主语言一致);本行已按 // 恢复");
        trim(line);
        if (!line[0]) continue;

        if (block[0] && skipping) {
            // F6:跳读按括号平衡吞到错误构造闭合,一构造一报
            for (const char* q2 = line; *q2; q2++) {
                if (*q2 == '{') bal++;
                if (*q2 == '}') bal--;
            }
            if (bal <= 0) skipping = 0;
            continue;
        }

        if (!block[0]) {
            if (line[0] == '[') {
                push(r, "Ctron.ctcl", "E5040", "本语言不用 [section] 段头;请用块:pkg { ... } / dep \"名\" { ... }");
                continue;
            }
            // 块头:IDENT [STR] {
            char id[32];
            int il = ctcl_ident(line, id, sizeof id);
            if (!il) {
                push(r, "Ctron.ctcl", "E5040", "块外只允许块头(NAME [\"名\"]) {");
                continue;
            }
            const char* rest_raw = line + il;
            const char* rs = rest_raw;
            while (*rs == ' ' || *rs == '\t') rs++;
            int has_arg = (rs[0] == '"');
            if (has_arg && rest_raw[0] != ' ' && rest_raw[0] != '\t')
                push(r, "Ctron.ctcl", "E5040", "块名与名字实参之间需要空格:dep \"名\"");
            const char* rest = rest_raw;
            while (*rest == ' ' || *rest == '\t') rest++;
            char arg[64] = {0};
            const char* after = rest;
            if (*rest == '"') {
                if (!ctcl_qtoken(rest, arg, sizeof arg, &after)) {
                    push(r, "Ctron.ctcl", "E5040", "非法的名字实参");
                    continue;
                }
                while (*after == ' ' || *after == '\t') after++;
            }
            if (*after != '{' || after[1] != '\0') {
                push(r, "Ctron.ctcl", "E5040", "块外只允许块头(NAME [\"名\"]) {");
                continue;
            }
            if (strcmp(id, "pkg") != 0 && strcmp(id, "comptime") != 0 && strcmp(id, "dep") != 0) {
                push(r, "Ctron.ctcl", "E5044", "未知块 %s;合法块:comptime, dep, pkg", id);
                snprintf(block, sizeof block, "skip");
                continue;
            }
            nseen = 0;
            if (strcmp(id, "dep") == 0) {
                if (!has_arg) {
                    push(r, "Ctron.ctcl", "E5041", "dep 是键控块:dep \"名\" { ... }");
                    snprintf(block, sizeof block, "skip");
                    continue;
                }
                if (!c_is_name(arg)) {
                    push(&pend, "Ctron.ctcl", "E5048", "键控块名 '%s' 不符合包名规则", arg);
                    snprintf(block, sizeof block, "skip");
                    continue;
                }
                int dupi = -1;
                for (int i = 0; i < ndeps; i++)
                    if (strcmp(seen_deps[i], arg) == 0) dupi = i;
                if (dupi >= 0)
                    push(&pend, "Ctron.ctcl", "E5045", "重复的 dep \"%s\"(首次在第 %d 行);同名块禁止追加", arg, seen_dep_ln[dupi]);
                else if (ndeps < CTCL_MAXKEYS) {
                    snprintf(seen_deps[ndeps], 64, "%s", arg);
                    seen_dep_ln[ndeps] = ln_num;
                    ndeps++;
                }
            }
            if (strcmp(id, "pkg") == 0 && has_arg) {
                push(r, "Ctron.ctcl", "E5041", "pkg 是记录块,不带名字实参");
                snprintf(block, sizeof block, "skip");
                continue;
            }
            if (strcmp(id, "pkg") == 0) b_saw_pkg = 1;
            if (strcmp(id, "dep") == 0) {
                dep_active = 1;
                has_path = has_git = has_rev = has_version = 0;
            }
            snprintf(block, sizeof block, "%s", id);
            continue;
        }

        if (strcmp(line, "}") == 0) {
            for (size_t qi = 0; qi < pend.n; qi++) {
                push(r, pend.d[qi].rel, pend.d[qi].code, "%s", pend.d[qi].msg);
            }
            if (pend.n) {
                free(pend.d);
                pend.d = NULL;
                pend.n = 0;
            }
            if (b_saw_pkg) saw_pkg = 1;
            if (b_has_mver) has_mver = 1;
            if (b_mver_ok) mver_ok = 1;
            if (b_saw_name) saw_name = 1;
            if (b_saw_version) saw_version = 1;
            if (b_comptime_seen) p->has_comptime = 1;
            if (b_budget_ok) {
                p->budget_ok = 1;
                p->budget_ms = b_budget;
            }
            b_saw_pkg = b_has_mver = b_mver_ok = b_saw_name = b_saw_version = 0;
            b_comptime_seen = b_budget_ok = 0;
            if (dep_active) {
                const char* labels[3] = {"path", "git+rev", "version"};
                int g[3] = {has_path, has_git && has_rev, has_version};
                int present[3], np = 0;
                for (int gi = 0; gi < 3; gi++)
                    if (g[gi]) present[np++] = gi;
                if (np >= 2)
                    push(r, "Ctron.ctcl", "E5049", "dep 来源互斥:%s 与 %s 同现", labels[present[0]], labels[present[1]]);
                else if (np == 0)
                    push(r, "Ctron.ctcl", "E5049", "dep 需要且仅需要一种来源:path | git+rev | version");
                dep_active = 0;
            }
            block[0] = '\0';
            continue;
        }

        if (strchr(line, '{') || strchr(line, '}')) {
            push(r, "Ctron.ctcl", "E5040", "块内禁止嵌套块/单行块(深度恒 1);此块已被跳过");
            skipping = 1;
            bal = 0;
            for (const char* q2 = line; *q2; q2++) {
                if (*q2 == '{') bal++;
                if (*q2 == '}') bal--;
            }
            continue;
        }

        // 字段:IDENT = value
        char* eq = strchr(line, '=');
        if (!eq) {
            push(r, "Ctron.ctcl", "E5040", "块内每行必须是 键 = 值");
            continue;
        }
        *eq = '\0';
        char key[32];
        if (!ctcl_ident(line, key, sizeof key)) {
            push(r, "Ctron.ctcl", "E5040", "块内每行必须是 键 = 值");
            continue;
        }
        char* v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;
        char vc[1024];
        char sv[1024];
        long iv = 0;
        int vkind = -1; // 0=str 1=int 2=bool 3=list;-1=非法(已诊断)
        char litems[16][128];
        int nlitems = 0;

        // 值词法分类(顺序镜像 Python parse_value;E5048/E5040 于本阶段产出)
        if (strcmp(v, "true") == 0) {
            vkind = 2;
        } else if (strcmp(v, "false") == 0) {
            vkind = 2;
        } else if (v[0] == '\'') {
            push(&pend, "Ctron.ctcl", "E5048", "不支持单引号字符串(唯一拼写:双引号)");
        } else if (v[0] == '"') {
            if (ctcl_str(v, sv, sizeof sv)) {
                vkind = 0;
                snprintf(vc, sizeof vc, "%s", sv);
            } else {
                push(&pend, "Ctron.ctcl", "E5048", "非法字符串值(转义只允许 反斜杠加引号 与 双反斜杠)");
            }
        } else if (ctcl_is_int(v)) {
            vkind = 1;
            iv = strtol(v, NULL, 10);
        } else if (ctcl_is_float(v)) {
                push(r, "Ctron.ctcl", "E5040", "不支持浮点;数值配置一律定点整数(如 85 表 85%%)");
        } else if (v[0] == '{') {
            push(r, "Ctron.ctcl", "E5040", "不支持内联表;复杂记录请用键控块表达(块 \"名\" { ... })");
        } else if (v[0] == '[') {
            size_t vn = strlen(v);
            if (v[vn - 1] != ']') {
                push(r, "Ctron.ctcl", "E5040", "列表必须单行且以 ] 结尾;若元素是复杂结构,请改用键控块(块 \"名\" { ... })而非多行列表");
            } else {
                char buf[1024];
                snprintf(buf, sizeof buf, "%.*s", (int)(vn - 2), v + 1);
                size_t bl = strlen(buf);
                while (bl && (buf[bl - 1] == ' ' || buf[bl - 1] == '\t')) buf[--bl] = '\0';
                if (bl == 0) {
                    vkind = 3;
                } else if (buf[bl - 1] == ',') {
                    push(&pend, "Ctron.ctcl", "E5048", "列表不允许尾逗号(最后元素后直接 ']')");
                } else {
                    int bad = 0;
                    char* tok = strtok(buf, ",");
                    while (tok) {
                        trim(tok);
                        char cap2[128];
                        if (*tok && ctcl_str(tok, cap2, sizeof cap2)) {
                            if (nlitems < 16) snprintf(litems[nlitems++], 128, "%s", cap2);
                        } else {
                            push(&pend, "Ctron.ctcl", "E5048", "列表元素必须是双引号字符串;复杂结构请用键控块");
                            bad = 1;
                            break;
                        }
                        tok = strtok(NULL, ",");
                    }
                    if (!bad) vkind = 3;
                }
            }
        } else {
            push(r, "Ctron.ctcl", "E5040", "无法识别的值:%s", v);
        }

        int dup = 0;
        for (int i = 0; i < nseen; i++)
            if (strcmp(seen_keys[i], key) == 0) dup = 1;
        if (dup) {
            push(&pend, "Ctron.ctcl", "E5045", "重复键 %s(同名键只允许一次)", key);
            continue;
        }
        if (nseen < CTCL_MAXKEYS) snprintf(seen_keys[nseen++], 32, "%s", key);

        if (strcmp(block, "pkg") == 0) {
            if (strcmp(key, "manifest_version") == 0) {
                b_has_mver = 1;
                if (vkind == 1) {
                    b_mver_ok = (iv == 1);
                    if (!b_mver_ok)
                        push(&pend, "Ctron.ctcl", "E5050", "manifest_version 必须为 1");
                } else if (vkind >= 0) {
                    push(&pend, "Ctron.ctcl", "E5046", "manifest_version 的类型应为 int");
                }
            } else if (strcmp(key, "name") == 0) {
                b_saw_name = 1;
                if (vkind == 0) {
                    free(p->pkg_name);
                    p->pkg_name = strdup(vc);
                    if (!c_is_name(vc))
                        push(&pend, "Ctron.ctcl", "E5046", "键 name 值 '%s' 不符合 %s", vc, NAME_PAT);
                } else if (vkind > 0) {
                    push(&pend, "Ctron.ctcl", "E5046", "name 的类型应为 str");
                }
            } else if (strcmp(key, "version") == 0) {
                b_saw_version = 1;
                if (vkind == 0) {
                    free(p->pkg_version);
                    p->pkg_version = strdup(vc);
                    if (!c_is_semver(vc))
                        push(&pend, "Ctron.ctcl", "E5046", "键 version 值 '%s' 不符合 %s", vc, SEMVER_PAT);
                } else if (vkind > 0) {
                    push(&pend, "Ctron.ctcl", "E5046", "version 的类型应为 str");
                }
            } else if (strcmp(key, "caps") == 0) {
                if (vkind == 3) {
                    for (int i2 = 0; i2 < nlitems; i2++) {
                        int known = strcmp(litems[i2], "fs") == 0 || strcmp(litems[i2], "time") == 0;
                        if (!known) {
                            push(&pend, "Ctron.ctcl", "E5043", "未知能力 %s;合法:fs, time(能力是安全边界,未知即拒绝)", litems[i2]);
                        } else {
                            int already = 0;
                            for (size_t c = 0; c < p->ncaps; c++)
                                if (strcmp(p->caps[c], litems[i2]) == 0) already = 1;
                            if (!already) {
                                p->caps = (char**)realloc(p->caps, (p->ncaps + 1) * sizeof(char*));
                                p->caps[p->ncaps++] = strdup(litems[i2]);
                            }
                        }
                    }
                } else if (vkind >= 0) {
                    push(&pend, "Ctron.ctcl", "E5046", "caps 的类型应为 list");
                }
            } else {
                char hint[128] = {0}, close[32];
                if (ctcl_close(key, PKG_KEYS, 4, close, sizeof close))
                    snprintf(hint, sizeof hint, ";你是不是想要 %s?", close);
                push(&pend, "Ctron.ctcl", "E5043", "块 pkg 中未知键 %s%s;合法键:manifest_version, name, version, caps", key, hint);
            }
        } else if (strcmp(block, "comptime") == 0) {
            if (strcmp(key, "budget_ms") == 0) {
                b_comptime_seen = 1;
                if (vkind == 1) {
                    b_budget = iv;
                    b_budget_ok = 1;
                    if (iv < 1)
                        push(&pend, "Ctron.ctcl", "E5046", "键 budget_ms 必须 >= 1");
                } else if (vkind >= 0) {
                    push(&pend, "Ctron.ctcl", "E5046", "budget_ms 的类型应为 int");
                }
            } else {
                push(&pend, "Ctron.ctcl", "E5043", "块 comptime 中未知键 %s;合法键:budget_ms", key);
            }
        } else if (strcmp(block, "dep") == 0) {
            if (strcmp(key, "path") == 0) has_path = 1;
            else if (strcmp(key, "git") == 0) has_git = 1;
            else if (strcmp(key, "rev") == 0) has_rev = 1;
            else if (strcmp(key, "version") == 0) has_version = 1;
            else push(&pend, "Ctron.ctcl", "E5043", "块 dep 中未知键 %s;合法键:path, git, rev, version", key);
        }
    }

    if (block[0])
        push(r, "Ctron.ctcl", "E5040", "块未闭合(缺 })");
    if (!saw_pkg) {
        push(r, "Ctron.ctcl", "E5047", "缺 pkg 块");
    } else {
        if (!has_mver)
            push(r, "Ctron.ctcl", "E5050", "pkg 缺语言版本键 manifest_version(必须存在且 = 1)");
        else if (!mver_ok)
            push(r, "Ctron.ctcl", "E5050", "manifest_version 必须为 1");
        if (!saw_name) push(r, "Ctron.ctcl", "E5047", "pkg 缺必填键 name");
        if (!saw_version) push(r, "Ctron.ctcl", "E5047", "pkg 缺必填键 version");
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
    free(p->pkg_version);
    for (size_t i = 0; i < p->ncaps; i++) free(p->caps[i]);
    free(p->caps);
    memset(p, 0, sizeof *p);
}

static void pkg_load(pkg* p, const char* root, pkg_res* r) {
    char mpath[4096];
    snprintf(mpath, sizeof mpath, "%s/Ctron.ctcl", root);
    ctcl_load(p, mpath, r);
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
    // 导入的 std 能力名:use std.<key>.<Name>(key ∈ {fs,time,net,db},§8.2)
    const cfile* f = m->pr.file;
    for (size_t j = 0; j < f->ndecls; j++) {
        const cdecl* d = &f->decls[j];
        if (d->kind != D_USE) continue;
        for (size_t k = 0; k < d->use.nimports; k++) {
            const cimport* imp = &d->use.imports[k];
            if (imp->nsegs != 3 || strcmp(imp->segs[0], "std") != 0) continue;
            const char* key = imp->segs[1];
            if (strcmp(key, "fs") != 0 && strcmp(key, "time") != 0 &&
                strcmp(key, "net") != 0 && strcmp(key, "db") != 0) continue;
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
                            push(r, m->rel, "E4010", "使用 %s 能力超出清单(caps)声明:参数 &%s", key, name);
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
    pkg_load(&p, root, &r);
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

// —— CTCL 单文件检查(manifest 子命令)——
ctron_manifest ctron_manifest_check(const char* path) {
    ctron_manifest m = {0};
    pkg p = {0};
    pkg_res r = {0};
    ctcl_load(&p, path, &r);
    m.name = p.pkg_name; p.pkg_name = NULL;
    m.version = p.pkg_version; p.pkg_version = NULL;
    m.caps = p.caps; m.ncaps = p.ncaps; p.caps = NULL; p.ncaps = 0;
    m.has_comptime = p.has_comptime;
    m.budget_ok = p.budget_ok;
    m.budget_ms = p.budget_ms;
    m.diags = r;
    pkg_free(&p);
    return m;
}

void ctron_manifest_free(ctron_manifest* m) {
    if (!m) return;
    free(m->name);
    free(m->version);
    for (size_t i = 0; i < m->ncaps; i++) free(m->caps[i]);
    free(m->caps);
    ctron_pkg_res_free(&m->diags);
    memset(m, 0, sizeof *m);
}
