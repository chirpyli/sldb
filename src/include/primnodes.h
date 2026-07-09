#ifndef PRIMNODES_H
#define PRIMNODES_H

#include "nodes.h"
#include "pg_list.h"

/* 表/关系引用：FROM users */
typedef struct RangeVar {
    NodeTag type;
    char   *relname;
    char   *schemaname;
} RangeVar;

/* 列引用：对应 SQL 中的列名 */
typedef struct ColumnRef {
    NodeTag type;
    List   *fields;   /* String 节点列表，支持 t.col 形式 */
} ColumnRef;

/* 结果目标列：SELECT 后的列，支持别名 */
typedef struct ResTarget {
    NodeTag type;
    char   *name;     /* 列别名（可选） */
    Node   *val;      /* 列值表达式 */
} ResTarget;

typedef enum A_Expr_Kind {
    AEXPR_OP,
    AEXPR_AND,
    AEXPR_OR,
    AEXPR_NOT,
} A_Expr_Kind;

/* 标量表达式：a op b */
typedef struct A_Expr {
    NodeTag     type;
    A_Expr_Kind kind;
    Node       *lexpr;
    Node       *rexpr;
    int         oper;       /* 操作符（单字符 token 或多字符 token 常量） */
} A_Expr;

typedef enum ConstType {
    CONST_INT,
    CONST_STRING,
} ConstType;

/* 常量值 */
typedef struct A_Const {
    NodeTag   type;
    ConstType consttype;
    union {
        long  ival;
        char *sval;
    } val;
} A_Const;

/* 类型名：CREATE TABLE 中的 INT / TEXT / VARCHAR */
typedef struct TypeName {
    NodeTag type;
    char   *name;
} TypeName;

/* 列定义：CREATE TABLE 中的 colname type */
typedef struct ColumnDef {
    NodeTag  type;
    char    *colname;
    TypeName *typeName;
} ColumnDef;

typedef enum SortByDir {
    SORTBY_DEFAULT,
    SORTBY_ASC,
    SORTBY_DESC,
} SortByDir;

/* ORDER BY 子句 */
typedef struct SortBy {
    NodeTag   type;
    Node     *node;
    SortByDir sortby_dir;
} SortBy;

/* UPDATE SET 子句 */
typedef struct SetClause {
    NodeTag type;
    char   *name;
    Node   *val;
} SetClause;

/* 字符串包装节点（用于 List 中的字符串元素） */
typedef struct String {
    NodeTag type;
    char   *sval;
} String;

#endif
