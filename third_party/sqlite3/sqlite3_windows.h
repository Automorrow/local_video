/* SQLite3 Header for Windows Cross Compile */
#ifndef SQLITE3_H
#define SQLITE3_H
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

#define SQLITE_OK           0
#define SQLITE_ERROR        1
#define SQLITE_INTERNAL     2
#define SQLITE_PERM         3
#define SQLITE_ABORT        4
#define SQLITE_BUSY         5
#define SQLITE_LOCKED       6
#define SQLITE_NOMEM        7
#define SQLITE_READONLY     8
#define SQLITE_INTERRUPT    9
#define SQLITE_IOERR       10
#define SQLITE_CORRUPT     11
#define SQLITE_NOTFOUND    12
#define SQLITE_FULL        13
#define SQLITE_CANTOPEN    14
#define SQLITE_PROTOCOL    15
#define SQLITE_EMPTY       16
#define SQLITE_SCHEMA      17
#define SQLITE_TOOBIG      18
#define SQLITE_CONSTRAINT  19
#define SQLITE_MISMATCH    20
#define SQLITE_MISUSE      21
#define SQLITE_NOLFS       22
#define SQLITE_AUTH        23
#define SQLITE_FORMAT      24
#define SQLITE_RANGE       25
#define SQLITE_NOTADB      26
#define SQLITE_NOTICE      27
#define SQLITE_WARNING     28
#define SQLITE_ROW         100
#define SQLITE_DONE        101

#define SQLITE_INTEGER  1
#define SQLITE_FLOAT    2
#define SQLITE_BLOB     4
#define SQLITE_NULL     5
#define SQLITE_TEXT     3

int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_close(sqlite3 *db);
int sqlite3_exec(sqlite3 *db, const char *sql, int (*callback)(void*,int,char**,char**), void *arg, char **errmsg);
int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
int sqlite3_step(sqlite3_stmt *pStmt);
int sqlite3_finalize(sqlite3_stmt *pStmt);
int sqlite3_reset(sqlite3_stmt *pStmt);
int sqlite3_bind_int(sqlite3_stmt *stmt, int col, int val);
int sqlite3_bind_int64(sqlite3_stmt *stmt, int col, long long val);
int sqlite3_bind_text(sqlite3_stmt *stmt, int col, const char *text, int len, void(*destructor)(void*));
int sqlite3_bind_null(sqlite3_stmt *stmt, int col);
int sqlite3_column_int(sqlite3_stmt *stmt, int col);
long long sqlite3_column_int64(sqlite3_stmt *stmt, int col);
double sqlite3_column_double(sqlite3_stmt *stmt, int col);
const char *sqlite3_column_text(sqlite3_stmt *stmt, int col);
const void *sqlite3_column_blob(sqlite3_stmt *stmt, int col);
int sqlite3_column_bytes(sqlite3_stmt *stmt, int col);
int sqlite3_column_type(sqlite3_stmt *stmt, int col);
int sqlite3_column_count(sqlite3_stmt *pStmt);
const char *sqlite3_column_name(sqlite3_stmt *pStmt, int N);
const char *sqlite3_errmsg(sqlite3 *db);
void sqlite3_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_H */
