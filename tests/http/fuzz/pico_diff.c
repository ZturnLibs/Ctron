/* pico_diff.c —— P4-D fuzz 差分对拍侧(测试线;不入产物)
 *
 * stdin 读 fuzz_http.ct 差分模式(CTRON_FUZZ_DIFF=1)吐出的逐 case 行:
 *     d <our_rc> <hex>        (our_rc ∈ -1|0|1 = ctron http_parse_head 判)
 * 每行 hex 解码后喂 picohttpparser:先 phr_parse_request,败则
 * phr_parse_response(与 bench 语料面同构,任一 >0 即 pico 判"完整接受")。
 *
 * 计数:
 *   total       样本数
 *   ours_ok     ctron rc==1
 *   pico_ok     pico 接受
 *   pico_only   pico 接受而 ctron 不完整/拒 —— 严格子集的预期差(RFC 9110
 *               从严档:obs-fold/TE+CL/重复 CL/上限/裸 LF 等),登记面,非 fail
 *   ours_only   ctron 完整而 pico 拒 —— 应恒 0(>0 即异常,登记排查)
 * 构建:cc -O1 -w -o pico_diff pico_diff.c ../bench/pico/picohttpparser.c
 * 退出码:0 恒(计数归 runner 汇总;本工具自身不判门)。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../bench/pico/picohttpparser.h"

static int hexval(int c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

int main(void) {
    static char line[8192];
    static unsigned char buf[4096];
    long total = 0, ours_ok = 0, pico_ok = 0, pico_only = 0, ours_only = 0;
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "d ", 2) != 0) { continue; }
        char *p = line + 2;
        char *end = NULL;
        long ourrc = strtol(p, &end, 10);
        if (end == p) { continue; }
        p = end;
        while (*p == ' ') { p++; }
        size_t n = 0;
        while (hexval((unsigned char)p[0]) >= 0 && hexval((unsigned char)p[1]) >= 0 && n < sizeof(buf)) {
            buf[n++] = (unsigned char)(hexval((unsigned char)p[0]) * 16 + hexval((unsigned char)p[1]));
            p += 2;
        }
        total++;
        if (ourrc == 1) { ours_ok++; }

        const char *method, *path, *msg;
        size_t method_len, path_len, msg_len, num_headers;
        int minor_version, status;
        struct phr_header headers[32];
        num_headers = 32;
        ssize_t r = phr_parse_request((const char *)buf, n, &method, &method_len, &path, &path_len,
                                      &minor_version, headers, &num_headers, 0);
        if (r <= 0) {
            num_headers = 32;
            r = phr_parse_response((const char *)buf, n, &minor_version, &status, &msg, &msg_len,
                                   headers, &num_headers, 0);
        }
        if (r > 0) {
            pico_ok++;
            if (ourrc != 1) { pico_only++; }
        } else if (ourrc == 1) {
            ours_only++;
        }
    }
    printf("PICO total=%ld ours_ok=%ld pico_ok=%ld pico_accepts_we_reject=%ld we_accept_pico_rejects=%ld\n",
           total, ours_ok, pico_ok, pico_only, ours_only);
    return 0;
}
