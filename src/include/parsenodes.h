#ifndef PARSENODES_H
#define PARSENODES_H

#include "nodes.h"
#include "pg_list.h"
#include "primnodes.h"

/* 原始语句包装器 */
typedef struct RawStmt {
    NodeTag type;
    Node   *stmt;
    int     stmt_len;
} RawStmt;

typedef struct SelectStmt {
    NodeTag type;
    List   *targetList;   /* ResTarget 列表 */
    List   *fromClause;   /* RangeVar 列表 */
    Node   *whereClause;  /* WHERE 表达式，可为 NULL */
    List   *sortClause;   /* SortBy 列表 */
    Node   *limitCount;   /* LIMIT 值，可为 NULL */
} SelectStmt;

typedef struct InsertStmt {
    NodeTag  type;
    RangeVar *relation;
    List     *cols;       /* ResTarget 列表（列名），可为 NULL */
    List     *values;     /* 值表达式列表 */
} InsertStmt;

typedef struct UpdateStmt {
    NodeTag  type;
    RangeVar *relation;
    List     *targetList; /* SetClause 列表 */
    Node     *whereClause;
} UpdateStmt;

typedef struct DeleteStmt {
    NodeTag  type;
    RangeVar *relation;
    Node     *whereClause;
} DeleteStmt;

typedef struct CreateStmt {
    NodeTag  type;
    RangeVar *relation;
    List     *tableElts;  /* ColumnDef 列表 */
} CreateStmt;

typedef struct DropStmt {
    NodeTag type;
    List   *objects;      /* RangeVar 列表 */
    int     removeType;
} DropStmt;

#endif
