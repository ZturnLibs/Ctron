/*
 * ctron_sqlite.c —— G6 sqlite 门面垫片(出参翻返回值;ctron_dbpg_connect 直返 fd 同形)
 *
 * 契约(emit 链接面;与 vendored sqlite3.c 同批并链,链接序任意):
 *   int64_t ctron_sqlite_open(const char* path, int64_t flags) → 句柄(0=败;rc 入静态槽)
 *   int64_t ctron_sqlite_prepare(int64_t db, const char* sql)  → 句柄(0=败;rc 入静态槽)
 *   int64_t ctron_sqlite_last_rc(void)                         → 最近 open/prepare 的 sqlite rc
 *
 * Str 跨界 = const char* 零拷直通(ctron_tls_write 既有登记形)。
 * sqlite3_open_v2/prepare_v2 的 T** 出参在 Ctron 侧无可变指针出参类型
 * (net bind.ct 出参形登记同源),翻 int64_t 返回值 + rc 静态槽;垫片只此
 * 两件,step/finalize/reset/bind/column/exec/errmsg/changes/rowid/close
 * 全部由门面直宣 sqlite3_* 原型,不经本件。失败路径 sqlite3 侧句柄可能
 * 非 NULL(OOM 边缘),垫片负责收尾防漏。
 */
#include "sqlite3.h"
#include <stdint.h>
#include <stddef.h>

static int64_t ctron_sqlite_rc_slot = 0;

int64_t ctron_sqlite_open(const char* path, int64_t flags) {
    sqlite3* db = NULL;
    int rc = sqlite3_open_v2(path, &db, (int)flags, NULL);
    ctron_sqlite_rc_slot = (int64_t)rc;
    if (rc != SQLITE_OK) {
        if (db != NULL) sqlite3_close_v2(db);
        return 0;
    }
    return (int64_t)(intptr_t)db;
}

int64_t ctron_sqlite_prepare(int64_t h, const char* sql) {
    sqlite3* db = (sqlite3*)(intptr_t)h;
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    ctron_sqlite_rc_slot = (int64_t)rc;
    if (rc != SQLITE_OK) {
        if (st != NULL) sqlite3_finalize(st);
        return 0;
    }
    return (int64_t)(intptr_t)st;
}

int64_t ctron_sqlite_last_rc(void) {
    return ctron_sqlite_rc_slot;
}
