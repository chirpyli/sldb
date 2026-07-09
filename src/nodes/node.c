#include "node.h"
#include "mem.h"
#include "nodes.h"
#include "primnodes.h"

void *makeNode_impl(NodeTag tag, size_t size) {
    Node *n = (Node *)arena_alloc0(current_arena, size);
    n->type = tag;
    return (void *)n;
}

String *makeString(char *str) {
    String *s = makeNode(String);
    s->sval = (str ? arena_strdup(current_arena, str) : NULL);
    return s;
}

A_Const *makeIntConst(long val) {
    A_Const *c = makeNode(A_Const);
    c->consttype = CONST_INT;
    c->val.ival = val;
    return c;
}

A_Const *makeStringConst(char *str) {
    A_Const *c = makeNode(A_Const);
    c->consttype = CONST_STRING;
    c->val.sval = (str ? arena_strdup(current_arena, str) : NULL);
    return c;
}

A_Const *makeNullConst(void) {
    A_Const *c = makeNode(A_Const);
    c->consttype = CONST_INT;
    c->val.ival = 0;
    return c;
}

A_Expr *makeAExpr(A_Expr_Kind kind, Node *lexpr, Node *rexpr, int oper) {
    A_Expr *e = makeNode(A_Expr);
    e->kind = kind;
    e->lexpr = lexpr;
    e->rexpr = rexpr;
    e->oper = oper;
    return e;
}

TypeName *makeTypeName(char *name) {
    TypeName *t = makeNode(TypeName);
    t->name = (name ? arena_strdup(current_arena, name) : NULL);
    return t;
}
