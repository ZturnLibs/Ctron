/*
 * baseline_libpq.c —— 简单查询回环 libpq 基线(P5-F Task 6 性能门;F-C)
 * 同构口径:connect(含服务端认证)→ N × PQexec("SELECT 1")(逐发逐收、
 * 逐结果校验 + 释放)→ 退出。N = argv[1](默认 1000);conninfo = argv[2]
 * (与 ctron 臂同一 DSN)。任何失败 exit 1。
 * 链接:cc baseline_libpq.c -lpq(-I/-L 由 perf/run.sh 探测注入)。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpq-fe.h>

int main(int argc, char **argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 1000;
    const char *conninfo = (argc > 2) ? argv[2]
        : "postgres://postgres:postgres@127.0.0.1:5432/postgres";
    PGconn *c = PQconnectdb(conninfo);
    if (c == NULL || PQstatus(c) != CONNECTION_OK) {
        fprintf(stderr, "libpq connect failed: %s\n", c ? PQerrorMessage(c) : "null");
        return 1;
    }
    for (int i = 0; i < n; i++) {
        PGresult *r = PQexec(c, "SELECT 1");
        if (r == NULL || PQresultStatus(r) != PGRES_TUPLES_OK || PQntuples(r) != 1
            || strcmp(PQgetvalue(r, 0, 0), "1") != 0) {
            fprintf(stderr, "query %d failed: %s\n", i, c ? PQerrorMessage(c) : "-");
            return 1;
        }
        PQclear(r);
    }
    PQfinish(c);
    printf("OK n=%d\n", n);
    return 0;
}
