#include "trans_internal.h"

// trans_rt.c —— 生成码助手发射(算术检查/字符串/容器/断言族)
// ================= 助手生成 =================
void wbounds(char* lo, char* hi, size_t n, int bits, int us) {
    if (us) {
        snprintf(lo, n, "0");
        snprintf(hi, n, "%lluULL", bits >= 64 ? 18446744073709551615ULL : (1ULL << bits) - 1);
    } else if (bits >= 64) {
        snprintf(lo, n, "(-9223372036854775807LL - 1)");
        snprintf(hi, n, "9223372036854775807LL");
    } else {
        snprintf(lo, n, "%lldLL", -(1LL << (bits - 1)));
        snprintf(hi, n, "%lldLL", (1LL << (bits - 1)) - 1);
    }
}
void emit_helper(tc* c, const char* name) {
    // name 形如 ctron_<fam>_<wl>;fam ∈ add/sub/mul/div/mod/neg/wadd/wsub/decl/print/assert_eq/assert_ne
    char fam[32], wl[16];
    if (!strcmp(name, "ctron_str_len")) { snprintf(fam, sizeof fam, "str_len"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_char_len")) { snprintf(fam, sizeof fam, "str_char_len"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_contains")) { snprintf(fam, sizeof fam, "str_contains"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_cmp")) { snprintf(fam, sizeof fam, "str_cmp"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_concat")) { snprintf(fam, sizeof fam, "str_concat"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_str_slice")) { snprintf(fam, sizeof fam, "str_slice"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_byte_at")) { snprintf(fam, sizeof fam, "byte_at"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_byte_slice")) { snprintf(fam, sizeof fam, "byte_slice"); wl[0] = 0; }
    else if (!strcmp(name, "ctron_idx")) { snprintf(fam, sizeof fam, "idx"); wl[0] = 0; }
    else if (!strncmp(name, "ctron_assert_eq_", 16)) {
        snprintf(fam, sizeof fam, "assert_eq");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (!strncmp(name, "ctron_assert_ne_", 16)) {
        snprintf(fam, sizeof fam, "assert_ne");
        snprintf(wl, sizeof wl, "%s", name + 16);
    } else if (sscanf(name, "ctron_%31[^_]_%15s", fam, wl) != 2) {
        return;
    }
    int bits = 32, us = 0, isf = 0, isb = 0;
    int nowl = wl[0] == 0;
    if (!nowl && !strcmp(wl, "f64")) isf = 1;
    else if (!nowl && !strcmp(wl, "b")) isb = 1;
    else if (!nowl) { us = wl[0] == 'u'; bits = atoi(wl + 1); }
    char ct[32], lo[48], hi[48];
    if (!isf && !isb && !nowl && bits > 0) {
        ty t = ty_int(bits, us);
        snprintf(ct, sizeof ct, "%s", ctype_of(t));
        wbounds(lo, hi, sizeof lo, bits, us);
    }
    sb* o = &c->head;
    if (!strcmp(fam, "add") || !strcmp(fam, "sub") || !strcmp(fam, "mul")
        || !strcmp(fam, "div") || !strcmp(fam, "mod") || !strcmp(fam, "cadd")
        || !strcmp(fam, "csub") || !strcmp(fam, "cmul") || !strcmp(fam, "cdiv")
        || !strcmp(fam, "cmod") || !strcmp(fam, "iadd") || !strcmp(fam, "isub")
        || !strcmp(fam, "imul") || !strcmp(fam, "idiv") || !strcmp(fam, "imod")
        || !strcmp(fam, "madd") || !strcmp(fam, "msub") || !strcmp(fam, "mmul")
        || !strcmp(fam, "mdiv") || !strcmp(fam, "mmod")) {
        int is_c = 0;
        size_t fln = strlen(fam);
        // 复合族是 4 字符(cadd/iadd/madd…);普通二元 add/sub/mul/div/mod 是 3 字符,
        // 不能按首字母 'm' 判别(否则 mul/mod 被误当成员赋值族,base 掉进兜底 '%')。
        if (fln == 4 && fam[0] == 'c') is_c = 1;      // cadd… → "(assign)"
        else if (fln == 4 && fam[0] == 'i') is_c = 2; // iadd… → "(idx assign)"
        else if (fln == 4 && fam[0] == 'm') is_c = 3; // madd… → "(member assign)"
        const char* base = is_c ? fam + 1 : fam;
        const char* op = !strcmp(base, "add") ? "+" : !strcmp(base, "sub") ? "-" : !strcmp(base, "mul") ? "*"
                        : !strcmp(base, "div") ? "/" : "%";
        const char* zmsg = !strcmp(base, "div") ? "division by zero (/)" : "division by zero (%)";
        char omsg[48];
        if (is_c == 1) snprintf(omsg, sizeof omsg, "integer overflow (assign)");
        else if (is_c == 2) snprintf(omsg, sizeof omsg, "integer overflow (idx assign)");
        else if (is_c == 3) snprintf(omsg, sizeof omsg, "integer overflow (member assign)");
        else snprintf(omsg, sizeof omsg, "integer overflow (%s)", op);
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (!strcmp(base, "div") || !strcmp(base, "mod"))
            sb_f(o, "    if (b == 0) ctron_panic(\"%s\");\n", zmsg);
        sb_f(o, "    __int128 r = (__int128)a %s (__int128)b;\n", op);
        sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"%s\");\n",
             ct, lo, ct, hi, omsg);
        sb_f(o, "    return (%s)r;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "neg")) {
        if (us) {
            sb_f(o, "static %s %s(int64_t a) { (void)a; ctron_panic(\"integer overflow (neg)\"); return (%s)0; }\n",
                 ct, name, ct);
        } else {
            sb_f(o, "static %s %s(int64_t a) {\n    __int128 r = -(__int128)a;\n", ct, name);
            sb_f(o, "    if (r < (%s)(%s) || r > (%s)(%s)) ctron_panic(\"integer overflow (neg)\");\n", ct, lo, ct, hi);
            sb_f(o, "    return (%s)r;\n}\n", ct);
        }
        return;
    }
    if (!strcmp(fam, "wadd") || !strcmp(fam, "wsub")) {
        const char* op = !strcmp(fam, "wadd") ? "+" : "-";
        sb_f(o, "static %s %s(int64_t a, int64_t b) {\n", ct, name);
        if (bits >= 64)
            sb_f(o, "    return (%s)(int64_t)((uint64_t)a %s (uint64_t)b);\n", ct, op);
        else {
            unsigned long long m = (1ULL << bits) - 1;
            sb_f(o, "    uint64_t r = ((uint64_t)a %s (uint64_t)b) & %lluULL;\n", op, m);
            if (us) sb_f(o, "    return (%s)r;\n", ct);
            else sb_f(o, "    return (%s)((int64_t)(r ^ %lluULL) - (int64_t)%lluULL);\n",
                      ct, (1ULL << (bits - 1)), (1ULL << (bits - 1)));
        }
        sb_f(o, "}\n");
        return;
    }
    if (!strcmp(fam, "str_len")) { sb_f(o, "static int64_t %s(const char* s) { return (%s)(s ? strlen(s) : 0); }\n", name, "int64_t"); return; }
    if (!strcmp(fam, "str_char_len")) {
        sb_f(o, "static int64_t %s(const char* s) { int64_t n = 0; const unsigned char* p = (const unsigned char*)(s ? s : \"\"); while (*p) { if ((*p & 0xC0) != 0x80) n++; p++; } return n; }\n", name);
        return;
    }
    if (!strcmp(fam, "str_contains")) {
        sb_f(o, "static int %s(const char* s, const char* x) { return strstr(s ? s : \"\", x ? x : \"\") != 0; }\n", name);
        return;
    }
    if (!strcmp(fam, "str_cmp")) {
        sb_f(o, "static int %s(const char* a, const char* b) { return strcmp(a ? a : \"\", b ? b : \"\"); }\n", name);
        return;
    }
    if (!strcmp(fam, "str_concat")) {
        sb_f(o, "static const char* %s(const char* a, const char* b) {\n"
              "    size_t n1 = a ? strlen(a) : 0, n2 = b ? strlen(b) : 0;\n"
              "    char* r = (char*)malloc(n1 + n2 + 1);\n"
              "    if (n1) memcpy(r, a, n1);\n"
              "    if (n2) memcpy(r + n1, b, n2);\n"
              "    r[n1 + n2] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "str_slice")) {
        sb_f(o, "static const char* %s(const char* s, int64_t lo, int64_t hi, int incl) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    int64_t l = lo < 0 ? 0 : lo;\n"
              "    int64_t h = incl ? hi + 1 : hi;\n"
              "    if (h > (int64_t)len) h = len;\n"
              "    if (l < (int64_t)len && ((unsigned char)s[l] & 0xC0) == 0x80) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    if (h < (int64_t)len && ((unsigned char)s[h] & 0xC0) == 0x80) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    if (l < 0 || h < l) ctron_panic(\"invalid utf8 boundary (utf8)\");\n"
              "    char* r = (char*)malloc((size_t)(h - l) + 1);\n"
              "    if (h > l) memcpy(r, s + l, (size_t)(h - l));\n"
              "    r[h - l] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "byte_at")) {
        sb_f(o, "static int %s(const char* s, int64_t i) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    if (i < 0 || (unsigned long long)i >= len) ctron_panic(\"index out of bounds\");\n"
              "    return (unsigned char)s[i];\n}\n", name);
        return;
    }
    if (!strcmp(fam, "byte_slice")) {
        sb_f(o, "static const char* %s(const char* s, int64_t a, int64_t b) {\n"
              "    size_t len = s ? strlen(s) : 0;\n"
              "    if (a < 0 || b > (int64_t)len || a > b) ctron_panic(\"byte_slice 越界\");\n"
              "    char* r = (char*)malloc((size_t)(b - a) + 1);\n"
              "    if (b > a) memcpy(r, s + a, (size_t)(b - a));\n"
              "    r[b - a] = 0;\n"
              "    return r;\n}\n", name);
        return;
    }
    if (!strcmp(fam, "fmt")) {
        if (!strcmp(wl, "i64") || !strcmp(wl, "str")) { /* 不会到这 */ }
        char pt[16] = "int64_t";
        if (us) snprintf(pt, sizeof pt, "uint64_t");
        if (!strcmp(wl, "i8") || !strcmp(wl, "i16") || !strcmp(wl, "i32") || !strcmp(wl, "i64"))
            sb_f(o, "static const char* %s(int64_t v) { char* r = (char*)malloc(32); snprintf(r, 32, \"%%lld\", (long long)v); return r; }\n", name);
        else if (us)
            sb_f(o, "static const char* %s(uint64_t v) { char* r = (char*)malloc(32); snprintf(r, 32, \"%%llu\", (unsigned long long)v); return r; }\n", name);
        return;
    }
    if (!strcmp(name, "ctron_fmt_f64")) {
        sb_f(o, "static const char* %s(double v) { char* r = (char*)malloc(64); if (v == (double)(long long)v) snprintf(r, 64, \"%%.1f\", v); else snprintf(r, 64, \"%%g\", v); return r; }\n", name);
        return;
    }
    if (!strcmp(name, "ctron_fmt_bool")) {
        sb_f(o, "static const char* %s(int v) { char* r = (char*)malloc(8); snprintf(r, 8, \"%%s\", v ? \"true\" : \"false\"); return r; }\n", name);
        return;
    }
    if (!strcmp(fam, "as")) {
        sb_f(o, "static %s %s(int64_t v) {\n", ct, name);
        if (us)
            sb_f(o, "    uint64_t r = (uint64_t)v & %lluULL;\n"
                "    return (%s)(int64_t)r;\n}\n", (1ULL << bits) - 1, ct);
        else if (bits < 64)
            sb_f(o, "    uint64_t r = (uint64_t)v & %lluULL;\n"
                "    return (%s)((int64_t)(r ^ %lluULL) - (int64_t)%lluULL);\n}\n",
                (1ULL << bits) - 1, ct, (1ULL << (bits - 1)), (1ULL << (bits - 1)));
        else
            sb_f(o, "    return (%s)v;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "idx")) {
        sb_f(o, "static int64_t %s(int64_t n, int64_t i) { if (i < 0 || i >= n) ctron_panic(\"index out of bounds\"); return i; }\n", name);
        return;
    }
    if (!strcmp(fam, "decl")) {
        sb_f(o, "static %s %s(int64_t v) {\n", ct, name);
        if (us)
            sb_f(o, "    if ((__int128)(uint64_t)v > (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n", hi);
        else
            sb_f(o, "    if ((__int128)v < (__int128)(%s) || (__int128)v > (__int128)(%s)) ctron_panic(\"integer overflow (decl)\");\n",
                 lo, hi);
        sb_f(o, "    return (%s)v;\n}\n", ct);
        return;
    }
    if (!strcmp(fam, "print") && !strcmp(wl, "str")) {
        sb_f(o, "static void %s(const char* v) { printf(\"%%s\", v ? v : \"\"); }\n", name);
        return;
    }
    if (!strcmp(fam, "print")) {
        if (isf)
            sb_f(o, "static void %s(double v) { char b[64]; if (v == (double)(long long)v) snprintf(b, sizeof b, \"%%.1f\", v); else snprintf(b, sizeof b, \"%%g\", v); printf(\"%%s\", b); }\n", name);
        else if (isb)
            sb_f(o, "static void %s(int v) { printf(\"%%s\", v ? \"true\" : \"false\"); }\n", name);
        else if (us)
            sb_f(o, "static void %s(uint64_t v) { printf(\"%%llu\", (unsigned long long)v); }\n", name);
        else
            sb_f(o, "static void %s(int64_t v) { printf(\"%%lld\", (long long)v); }\n", name);
        return;
    }
    if (!strncmp(name, "ctron_box_", 10)) {
        char k[48];
        snprintf(k, sizeof k, "%s", name + 10);
        if (!strcmp(k, "I32"))
            sb_f(o, "static int32_t* %s(int64_t v) { int32_t* p = (int32_t*)malloc(sizeof(int32_t)); *p = (int32_t)v; return p; }\n", name);
        else if (!strcmp(k, "I64"))
            sb_f(o, "static int64_t* %s(int64_t v) { int64_t* p = (int64_t*)malloc(sizeof(int64_t)); *p = v; return p; }\n", name);
        else if (!strcmp(k, "F64"))
            sb_f(o, "static double* %s(double v) { double* p = (double*)malloc(sizeof(double)); *p = v; return p; }\n", name);
        else if (!strcmp(k, "Bool"))
            sb_f(o, "static int* %s(int v) { int* p = (int*)malloc(sizeof(int)); *p = v; return p; }\n", name);
        else
            sb_f(o, "static ctron_t_%s* %s(ctron_t_%s v) { ctron_t_%s* p = (ctron_t_%s*)malloc(sizeof(ctron_t_%s)); *p = v; return p; }\n", k, name, k, k, k, k);
        return;
    }
    if (!strncmp(name, "ctron_simd_", 11)) {
        // Simd 域(C10-q):splat_N / toarr_N(元素统一 double,§9.5)
        int n = 0;
        if (!strncmp(name, "ctron_simd_splat_", 17)) n = atoi(name + 17);
        else if (!strncmp(name, "ctron_simd_toarr_", 17)) n = atoi(name + 17);
        if (n <= 0 || n > 64) return;
        char tn[48];
        snprintf(tn, sizeof tn, "ctron_simd_f64_%d", n);
        if (!strncmp(name, "ctron_simd_splat_", 17)) {
            sb_f(o, "static %s %s(double v) { %s r; for (int i = 0; i < %d; i++) r.d[i] = v; return r; }\n",
                 tn, name, tn, n);
        } else {
            use_arr(c, "f64"); // to_array 返回 f64 切片(typedef 在 arrs 段,先于 helpers)
            sb_f(o, "static ctron_arr_f64 %s(%s v) { ctron_arr_f64 r; r.n = %d; r.d = (double*)calloc((size_t)(%d ? %d : 1), sizeof(double)); for (int i = 0; i < %d; i++) r.d[i] = v.d[i]; return r; }\n",
                 name, tn, n, n, n, n);
        }
        return;
    }
    if (!strncmp(name, "ctron_new_", 10)) {
        // class 构造:字段逐个赋值
        char cn[48];
        snprintf(cn, sizeof cn, "%s", name + 10);
        cdef* cd = NULL;
        for (size_t i = 0; i < c->nclasses; i++)
            if (!strcmp(c->classes[i].name, cn)) { cd = &c->classes[i]; break; }
        if (!cd) return;
        sb sig = {0};
        sb_f(&sig, "static ctron_c_%s* %s(", cn, name);
        sb asg = {0};
        for (size_t j = 0; j < cd->n; j++) {
            ty ft = cd->fields[j].t;
            if (j) sb_s(&sig, ", ");
            sb_f(&sig, "%s f_%s", ctype_of(ft), cd->fields[j].name);
            sb_f(&asg, "    p->%s = f_%s;\n", cd->fields[j].name, cd->fields[j].name);
        }
        sb_f(o, "%s) {\n    ctron_c_%s* p = (ctron_c_%s*)calloc(1, sizeof(ctron_c_%s));\n", sig.d ? sig.d : "", cn, cn, cn);
        sb_s(o, asg.d ? asg.d : "");
        sb_f(o, "    return p;\n}\n");
        sb_free(&sig);
        sb_free(&asg);
        return;
    }
    if (!strncmp(name, "ctron_arr_", 10)) {
        // name: ctron_arr_<wl>_lit
        char wl[16];
        snprintf(wl, sizeof wl, "%s", name + 10);
        wl[strlen(wl) - 4] = 0; // 去掉 _lit
        if (!strcmp(wl, "str"))
            sb_f(o, "static ctron_arr_str %s(int64_t n, const char** vals) { ctron_arr_str r; r.n = n; r.d = (const char**)calloc((size_t)(n ? n : 1), sizeof(const char*)); for (int64_t i = 0; i < n; i++) r.d[i] = vals[i] ? vals[i] : \"\"; return r; }\n", name);
        else if (!strncmp(wl, "f", 1)) {
            sb_f(o, "static ctron_arr_%s %s(int64_t n, double* vals) { ctron_arr_%s r; r.n = n; r.d = (double*)calloc((size_t)(n ? n : 1), sizeof(double)); for (int64_t i = 0; i < n; i++) r.d[i] = vals[i]; return r; }\n",
                 wl, name, wl);
        } else {
            int ub = wl[0] == 'u';
            int bits = atoi(wl + 1);
            ty t = ty_int(bits, ub);
            if (bits < 64)
                sb_f(o, "static ctron_arr_%s %s(int64_t n, int64_t* vals) { ctron_arr_%s r; r.n = n; r.d = (%s*)calloc((size_t)(n ? n : 1), sizeof(%s)); for (int64_t i = 0; i < n; i++) r.d[i] = (%s)vals[i]; return r; }\n",
                     wl, name, wl, ctype_of(t), ctype_of(t), ctype_of(t));
            else
                sb_f(o, "static ctron_arr_%s %s(int64_t n, uint64_t* vals) { ctron_arr_%s r; r.n = n; r.d = (%s*)calloc((size_t)(n ? n : 1), sizeof(%s)); for (int64_t i = 0; i < n; i++) r.d[i] = (%s)vals[i]; return r; }\n",
                     wl, name, wl, ctype_of(t), ctype_of(t), ctype_of(t));
        }
        return;
    }
    if (!strcmp(fam, "assert_eq") || !strcmp(fam, "assert_ne")) {
        int ne = !strcmp(fam, "assert_ne");
        const char* label = ne ? "assert_ne failed" : "assert_eq failed";
        const char* cmp = ne ? "!=" : "==";
        if (!strcmp(wl, "str")) {
            sb_f(o, "static int %s(const char* a, const char* b) { if (!(strcmp(a ? a : \"\", b ? b : \"\") %s 0)) { char m[256]; snprintf(m, sizeof m, \"%s: %%s != %%s\", a ? a : \"\", b ? b : \"\"); ctron_panic(m); } return 1; }\n",
                 name, ne ? "!=" : "==", label);
        } else if (isf) {
            sb_f(o, "static int %s(double a, double b) {\n", name);
            sb_f(o, "    char x[64], y[64];\n");
            sb_f(o, "    if (a == (double)(long long)a) snprintf(x, sizeof x, \"%%.1f\", a); else snprintf(x, sizeof x, \"%%g\", a);\n");
            sb_f(o, "    if (b == (double)(long long)b) snprintf(y, sizeof y, \"%%.1f\", b); else snprintf(y, sizeof y, \"%%g\", b);\n");
            sb_f(o, "    if (!(a %s b)) { char m[160]; snprintf(m, sizeof m, \"%s: %%s != %%s\", x, y); ctron_panic(m); }\n    return 1;\n}\n",
                 cmp, label);
        } else if (isb) {
            sb_f(o, "static int %s(int a, int b) { if (!(a %s b)) ctron_panic(\"%s\"); return 1; }\n", name, cmp, label);
        } else {
            const char* pt = us ? "uint64_t" : "int64_t";
            sb_f(o, "static int %s(%s a, %s b) { if (!(a %s b)) { char m[128]; snprintf(m, sizeof m, \"%s: %%llu != %%llu\", (unsigned long long)a, (unsigned long long)b); ctron_panic(m); } return 1; }\n",
                 name, pt, pt, cmp, label);
        }
        return;
    }
}
