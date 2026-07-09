#include <stdio.h>
#include <string.h>
#include "mem.h"
#include "nodes.h"
#include "pg_list.h"
#include "primnodes.h"
#include "parsenodes.h"
#include "parser.h"
#include "gram.h"

/* flex 提供的输入文件指针 */
extern FILE *yyin;

static void indent(int n) {
    for (int i = 0; i < n; i++) putchar(' ');
}

static const char *op_str(int oper) {
    switch (oper) {
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '=': return "=";
        case '<': return "<";
        case '>': return ">";
        case OP_LTE: return "<=";
        case OP_GTE: return ">=";
        case OP_NEQ: return "<>";
        default: return "?";
    }
}

static void print_expr(Node *node) {
    if (!node) { printf("NULL"); return; }
    switch (node->type) {
    case T_ColumnRef: {
        ColumnRef *c = (ColumnRef *)node;
        printf("ColumnRef(");
        if (c->fields)
            for (int i = 0; i < list_length(c->fields); i++) {
                String *s = (String *)list_nth(c->fields, i);
                if (i) printf(".");
                printf("%s", s->sval ? s->sval : "");
            }
        printf(")");
        break;
    }
    case T_A_Const: {
        A_Const *c = (A_Const *)node;
        if (c->consttype == CONST_INT) printf("%ld", c->val.ival);
        else printf("'%s'", c->val.sval ? c->val.sval : "");
        break;
    }
    case T_A_Expr: {
        A_Expr *e = (A_Expr *)node;
        if (e->kind == AEXPR_NOT) {
            printf("(NOT "); print_expr(e->lexpr); printf(")");
        } else {
            const char *op = (e->kind == AEXPR_AND) ? "AND"
                           : (e->kind == AEXPR_OR)  ? "OR"
                           : op_str(e->oper);
            printf("("); print_expr(e->lexpr);
            printf(" %s ", op);
            print_expr(e->rexpr); printf(")");
        }
        break;
    }
    default:
        printf("<unknown-expr:%d>", node->type);
    }
}

static void print_restarget(ResTarget *r) {
    if (!r) { printf("NULL"); return; }
    if (r->name) { print_expr(r->val); printf(" AS %s", r->name); }
    else print_expr(r->val);
}

static void print_rangevar(RangeVar *rv) {
    if (!rv) { printf("NULL"); return; }
    printf("%s", rv->relname ? rv->relname : "");
}

static void print_sortby(SortBy *sb) {
    if (!sb) return;
    print_expr(sb->node);
    if (sb->sortby_dir == SORTBY_ASC) printf(" ASC");
    else if (sb->sortby_dir == SORTBY_DESC) printf(" DESC");
}

static void print_setclause(SetClause *sc) {
    if (!sc) return;
    printf("%s = ", sc->name ? sc->name : "");
    print_expr(sc->val);
}

static void print_columndef(ColumnDef *cd) {
    if (!cd) return;
    printf("%s %s", cd->colname ? cd->colname : "",
                     cd->typeName ? cd->typeName->name : "");
}

static void print_node(Node *node, int ind) {
    if (!node) { indent(ind); printf("NULL\n"); return; }
    switch (node->type) {
    case T_SelectStmt: {
        SelectStmt *s = (SelectStmt *)node;
        indent(ind); printf("SelectStmt {\n");
        indent(ind + 1); printf("targetList:\n");
        for (int i = 0; i < list_length(s->targetList); i++) {
            indent(ind + 2); print_restarget((ResTarget *)list_nth(s->targetList, i));
            printf("\n");
        }
        indent(ind + 1); printf("fromClause:\n");
        for (int i = 0; i < list_length(s->fromClause); i++) {
            indent(ind + 2); print_rangevar((RangeVar *)list_nth(s->fromClause, i));
            printf("\n");
        }
        if (s->whereClause) { indent(ind + 1); printf("where: "); print_expr(s->whereClause); printf("\n"); }
        if (s->sortClause) {
            indent(ind + 1); printf("orderBy:\n");
            for (int i = 0; i < list_length(s->sortClause); i++) {
                indent(ind + 2); print_sortby((SortBy *)list_nth(s->sortClause, i)); printf("\n");
            }
        }
        if (s->limitCount) { indent(ind + 1); printf("limit: "); print_expr(s->limitCount); printf("\n"); }
        indent(ind); printf("}\n");
        break;
    }
    case T_InsertStmt: {
        InsertStmt *s = (InsertStmt *)node;
        indent(ind); printf("InsertStmt {\n");
        indent(ind + 1); printf("relation: "); print_rangevar(s->relation); printf("\n");
        indent(ind + 1); printf("cols:\n");
        if (s->cols)
            for (int i = 0; i < list_length(s->cols); i++) {
                indent(ind + 2); print_restarget((ResTarget *)list_nth(s->cols, i)); printf("\n");
            }
        indent(ind + 1); printf("values:\n");
        for (int i = 0; i < list_length(s->values); i++) {
            indent(ind + 2); print_expr((Node *)list_nth(s->values, i)); printf("\n");
        }
        indent(ind); printf("}\n");
        break;
    }
    case T_UpdateStmt: {
        UpdateStmt *s = (UpdateStmt *)node;
        indent(ind); printf("UpdateStmt {\n");
        indent(ind + 1); printf("relation: "); print_rangevar(s->relation); printf("\n");
        indent(ind + 1); printf("set:\n");
        for (int i = 0; i < list_length(s->targetList); i++) {
            indent(ind + 2); print_setclause((SetClause *)list_nth(s->targetList, i)); printf("\n");
        }
        if (s->whereClause) { indent(ind + 1); printf("where: "); print_expr(s->whereClause); printf("\n"); }
        indent(ind); printf("}\n");
        break;
    }
    case T_DeleteStmt: {
        DeleteStmt *s = (DeleteStmt *)node;
        indent(ind); printf("DeleteStmt {\n");
        indent(ind + 1); printf("relation: "); print_rangevar(s->relation); printf("\n");
        if (s->whereClause) { indent(ind + 1); printf("where: "); print_expr(s->whereClause); printf("\n"); }
        indent(ind); printf("}\n");
        break;
    }
    case T_CreateStmt: {
        CreateStmt *s = (CreateStmt *)node;
        indent(ind); printf("CreateStmt {\n");
        indent(ind + 1); printf("relation: "); print_rangevar(s->relation); printf("\n");
        indent(ind + 1); printf("columns:\n");
        for (int i = 0; i < list_length(s->tableElts); i++) {
            indent(ind + 2); print_columndef((ColumnDef *)list_nth(s->tableElts, i)); printf("\n");
        }
        indent(ind); printf("}\n");
        break;
    }
    case T_DropStmt: {
        DropStmt *s = (DropStmt *)node;
        indent(ind); printf("DropStmt {\n");
        indent(ind + 1); printf("objects:\n");
        for (int i = 0; i < list_length(s->objects); i++) {
            indent(ind + 2); print_rangevar((RangeVar *)list_nth(s->objects, i)); printf("\n");
        }
        indent(ind); printf("}\n");
        break;
    }
    default:
        indent(ind); printf("<unknown stmt:%d>\n", node->type);
    }
}

int main(int argc, char **argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    } else {
        yyin = stdin;
    }

    Arena *arena = arena_create();
    set_current_arena(arena);

    ParserState pstate;
    memset(&pstate, 0, sizeof(pstate));
    g_parser_state = &pstate;

    int r = yyparse();
    if (r != 0 || pstate.error) {
        printf("Parse error: %s\n", pstate.err_msg);
        arena_destroy(arena);
        return 1;
    }

    if (pstate.parse_tree) {
        for (int i = 0; i < list_length(pstate.parse_tree); i++) {
            RawStmt *rs = (RawStmt *)list_nth(pstate.parse_tree, i);
            print_node(rs->stmt, 0);
            printf("\n");
        }
    } else {
        printf("(no statements)\n");
    }

    arena_destroy(arena);
    if (yyin != stdin) fclose(yyin);
    return 0;
}
