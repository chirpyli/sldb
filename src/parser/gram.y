%{
/*
 * gram.y - SLDB SQL 语法分析器（bison / LALR(1)）
 * 参考 PostgreSQL src/backend/parser/gram.y
 */
#include <stdio.h>
#include <string.h>
#include "parser.h"

/* 当前解析状态（classic 模式下的全局） */
ParserState *g_parser_state = NULL;

void yyerror(const char *msg) {
    snprintf(g_parser_state->err_msg, sizeof(g_parser_state->err_msg), "%s", msg);
    g_parser_state->error = 1;
}
%}

%code requires {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nodes.h"
#include "pg_list.h"
#include "primnodes.h"
#include "parsenodes.h"
#include "mem.h"
#include "node.h"
#include "parser.h"
}

%union {
    int          ival;
    char        *str;
    Node        *node;
    List        *list;
    SortByDir    sortdir;
    A_Expr_Kind  aexpr_kind;
    ConstType    consttype;
}

/* ===== 关键字 Token ===== */
%token KW_SELECT KW_FROM KW_WHERE KW_INSERT KW_INTO KW_VALUES
%token KW_UPDATE KW_SET KW_DELETE KW_CREATE KW_DROP KW_TABLE
%token KW_ORDER KW_BY KW_ASC KW_DESC KW_LIMIT KW_AS
%token KW_AND KW_OR KW_NOT KW_NULL
%token KW_INT KW_TEXT KW_VARCHAR

/* ===== 字面量 Token ===== */
%token <str>  IDENT
%token <ival> ICONST
%token <str>  SCONST

/* ===== 多字符运算符 Token ===== */
%token OP_LTE OP_GTE OP_NEQ
%token UMINUS

/* ===== 非终结符类型 ===== */
%type <node> SelectStmt InsertStmt UpdateStmt DeleteStmt CreateStmt DropStmt
%type <node> target_el opt_where_clause opt_limit_clause sortby
%type <node> set_clause column_def type_name c_expr b_expr a_expr
%type <list> target_list from_clause from_list opt_from_clause opt_sort_clause sort_clause
%type <list> insert_column_list column_ref_list value_list set_clause_list column_def_list
%type <str>  opt_alias
%type <sortdir> opt_asc_desc

/* ===== 运算符优先级与结合性（从低到高） ===== */
%left KW_OR
%left KW_AND
%right KW_NOT
%nonassoc '=' '<' '>' OP_LTE OP_GTE OP_NEQ
%left '+' '-'
%left '*' '/'
%right UMINUS

%start input

%%

/* ===================== 顶层 ===================== */

input:
    stmtlist
    ;

stmtlist:
    /* empty */
    | stmtlist stmt
    ;

stmt:
    SelectStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    | InsertStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    | UpdateStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    | DeleteStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    | CreateStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    | DropStmt ';'
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        g_parser_state->parse_tree = lappend(g_parser_state->parse_tree, rs);
    }
    ;

/* ===================== SELECT ===================== */

SelectStmt:
    KW_SELECT target_list
    opt_from_clause
    opt_where_clause
    opt_sort_clause
    opt_limit_clause
    {
        SelectStmt *s = makeNode(SelectStmt);
        s->targetList  = $2;
        s->fromClause  = $3;
        s->whereClause = $4;
        s->sortClause  = $5;
        s->limitCount  = $6;
        $$ = (Node *)s;
    }
    ;

target_list:
    target_el                  { $$ = makeList1($1); }
    | target_list ',' target_el { $$ = lappend($1, $3); }
    ;

target_el:
    a_expr opt_alias
    {
        ResTarget *res = makeNode(ResTarget);
        res->name = $2;
        res->val  = $1;
        $$ = (Node *)res;
    }
    | '*'
    {
        ColumnRef *col = makeNode(ColumnRef);
        col->fields = makeList1(makeString("*"));
        ResTarget *res = makeNode(ResTarget);
        res->name = NULL;
        res->val  = (Node *)col;
        $$ = (Node *)res;
    }
    ;

opt_alias:
    KW_AS IDENT    { $$ = $2; }
    | IDENT        { $$ = $1; }
    | /* empty */  { $$ = NULL; }
    ;

from_clause:
    from_list      { $$ = $1; }
    ;

/* FROM 可选：支持纯常量 SELECT（如 SELECT 1+2*3;）——Phase 1 端到端验证需要 */
opt_from_clause:
    KW_FROM from_clause   { $$ = $2; }
    | /* empty */         { $$ = NULL; }
    ;

from_list:
    IDENT
    {
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $1;
        $$ = makeList1(rv);
    }
    | from_list ',' IDENT
    {
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        $$ = lappend($1, rv);
    }
    ;

opt_where_clause:
    KW_WHERE a_expr   { $$ = $2; }
    | /* empty */     { $$ = NULL; }
    ;

opt_sort_clause:
    KW_ORDER KW_BY sort_clause   { $$ = $3; }
    | /* empty */                { $$ = NULL; }
    ;

sort_clause:
    sortby                    { $$ = makeList1($1); }
    | sort_clause ',' sortby  { $$ = lappend($1, $3); }
    ;

sortby:
    a_expr opt_asc_desc
    {
        SortBy *sb = makeNode(SortBy);
        sb->node = $1;
        sb->sortby_dir = $2;
        $$ = (Node *)sb;
    }
    ;

opt_asc_desc:
    KW_ASC        { $$ = SORTBY_ASC; }
    | KW_DESC     { $$ = SORTBY_DESC; }
    | /* empty */ { $$ = SORTBY_DEFAULT; }
    ;

opt_limit_clause:
    KW_LIMIT a_expr   { $$ = $2; }
    | /* empty */     { $$ = NULL; }
    ;

/* ===================== INSERT ===================== */

InsertStmt:
    KW_INSERT KW_INTO IDENT insert_column_list KW_VALUES '(' value_list ')'
    {
        InsertStmt *s = makeNode(InsertStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation = rv;
        s->cols = $4;
        s->values = $7;
        $$ = (Node *)s;
    }
    ;

insert_column_list:
    '(' column_ref_list ')'   { $$ = $2; }
    | /* empty */             { $$ = NULL; }
    ;

column_ref_list:
    IDENT
    {
        ResTarget *res = makeNode(ResTarget);
        res->name = $1;
        res->val  = NULL;
        $$ = makeList1(res);
    }
    | column_ref_list ',' IDENT
    {
        ResTarget *res = makeNode(ResTarget);
        res->name = $3;
        res->val  = NULL;
        $$ = lappend($1, res);
    }
    ;

value_list:
    a_expr                   { $$ = makeList1($1); }
    | value_list ',' a_expr  { $$ = lappend($1, $3); }
    ;

/* ===================== UPDATE ===================== */

UpdateStmt:
    KW_UPDATE IDENT KW_SET set_clause_list opt_where_clause
    {
        UpdateStmt *s = makeNode(UpdateStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $2;
        s->relation   = rv;
        s->targetList = $4;
        s->whereClause = $5;
        $$ = (Node *)s;
    }
    ;

set_clause_list:
    set_clause                    { $$ = makeList1($1); }
    | set_clause_list ',' set_clause { $$ = lappend($1, $3); }
    ;

set_clause:
    IDENT '=' a_expr
    {
        SetClause *sc = makeNode(SetClause);
        sc->name = $1;
        sc->val  = $3;
        $$ = (Node *)sc;
    }
    ;

/* ===================== DELETE ===================== */

DeleteStmt:
    KW_DELETE KW_FROM IDENT opt_where_clause
    {
        DeleteStmt *s = makeNode(DeleteStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation    = rv;
        s->whereClause = $4;
        $$ = (Node *)s;
    }
    ;

/* ===================== CREATE TABLE ===================== */

CreateStmt:
    KW_CREATE KW_TABLE IDENT '(' column_def_list ')'
    {
        CreateStmt *s = makeNode(CreateStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation  = rv;
        s->tableElts = $5;
        $$ = (Node *)s;
    }
    ;

column_def_list:
    column_def                       { $$ = makeList1($1); }
    | column_def_list ',' column_def { $$ = lappend($1, $3); }
    ;

column_def:
    IDENT type_name
    {
        ColumnDef *cd = makeNode(ColumnDef);
        cd->colname  = $1;
        cd->typeName = (TypeName *)$2;
        $$ = (Node *)cd;
    }
    ;

type_name:
    KW_INT       { $$ = (Node *)makeTypeName("INT"); }
    | KW_TEXT    { $$ = (Node *)makeTypeName("TEXT"); }
    | KW_VARCHAR { $$ = (Node *)makeTypeName("VARCHAR"); }
    ;

/* ===================== DROP TABLE ===================== */

DropStmt:
    KW_DROP KW_TABLE IDENT
    {
        DropStmt *s = makeNode(DropStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->objects    = makeList1(rv);
        s->removeType = 0;
        $$ = (Node *)s;
    }
    ;

/* ===================== 表达式（三层优先级） ===================== */

/* c_expr: 原子表达式 */
c_expr:
    IDENT
    {
        ColumnRef *col = makeNode(ColumnRef);
        col->fields = makeList1(makeString($1));
        $$ = (Node *)col;
    }
    | ICONST
    {
        $$ = (Node *)makeIntConst($1);
    }
    | SCONST
    {
        $$ = (Node *)makeStringConst($1);
    }
    | KW_NULL
    {
        $$ = (Node *)makeNullConst();
    }
    | '(' a_expr ')'
    {
        $$ = $2;
    }
    ;

/* b_expr: 算术表达式 */
b_expr:
    c_expr                { $$ = $1; }
    | b_expr '+' b_expr   { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '+'); }
    | b_expr '-' b_expr   { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '-'); }
    | b_expr '*' b_expr   { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '*'); }
    | b_expr '/' b_expr   { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '/'); }
    ;

/* a_expr: 比较与逻辑表达式 */
a_expr:
    b_expr                            { $$ = $1; }
    | a_expr '=' a_expr               { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '='); }
    | a_expr '<' a_expr               { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '<'); }
    | a_expr '>' a_expr               { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '>'); }
    | a_expr OP_LTE a_expr            { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_LTE); }
    | a_expr OP_GTE a_expr            { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_GTE); }
    | a_expr OP_NEQ a_expr            { $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_NEQ); }
    | a_expr KW_AND a_expr            { $$ = (Node *)makeAExpr(AEXPR_AND, $1, $3, 0); }
    | a_expr KW_OR a_expr             { $$ = (Node *)makeAExpr(AEXPR_OR, $1, $3, 0); }
    | KW_NOT a_expr %prec KW_NOT      { $$ = (Node *)makeAExpr(AEXPR_NOT, $2, NULL, 0); }
    | '-' a_expr %prec UMINUS         { $$ = (Node *)makeAExpr(AEXPR_OP,
                                            (Node *)makeIntConst(0), $2, '-'); }
    ;

%%
