/*
 * eval.c - 表达式求值器（对标 PostgreSQL execExpr.c 的简化递归版）
 *
 * 本文件实现 Phase 1 的"类型系统 + 常量表达式求值"：
 *   - BuiltinType[] 是写死在 C 层的内建类型表（类型系统的真源，不依赖系统表）
 *   - eval_expr 递归遍历 primnodes.h 表达式树，完成算术/比较/逻辑求值
 *   - 除零 / 类型不匹配统一 ereport(SLDB_ERR_TYPE, ...)
 *
 * 关键：oper 取自解析层 gram.y 的 token（'+' '-' '*' '/' 或 OP_LTE 等），
 * 算术与比较在 AST 上同属 AEXPR_OP，靠 oper 值区分（详见设计文档 §5.4）。
 */
#include "types.h"
#include "primnodes.h"
#include "error.h"
#include "mem.h"
#include "gram.h"       /* OP_LTE / OP_GTE / OP_NEQ 等多字符 token 常量 */
#include <string.h>     /* strcmp */
#include <strings.h>    /* strcasecmp */
#include <stdlib.h>     /* memset */

/* ---------------- 内建类型表（类型系统的真源，写死） ---------------- */
static const BuiltinType builtin_types[] = {
    { "int",     T_INT,  8 },
    { "integer", T_INT,  8 },
    { "text",    T_TEXT, -1 },
    { "varchar", T_TEXT, -1 },  /* 首版 VARCHAR 等价于 TEXT（长度限制在后续约束阶段） */
    { "bool",    T_BOOL, 1 },
    { "boolean", T_BOOL, 1 },
};

DataType typename_to_datatype(const struct TypeName *tn)
{
    if (!tn || !tn->name) return T_INVALID;
    for (size_t i = 0; i < sizeof(builtin_types) / sizeof(builtin_types[0]); i++) {
        if (strcasecmp(builtin_types[i].name, tn->name) == 0)
            return builtin_types[i].type;
    }
    return T_INVALID;   /* 未知类型：交由调用方报错（如 Phase 3 CREATE TABLE） */
}

/* ---------------- 内部辅助（static） ---------------- */
static Value eval_arith_cmp(int oper, const Value *l, const Value *r);
static Value eval_logical(const A_Expr *e);

/* 构造一个 T_INT 值 */
static Value make_int(int64_t i)
{
    Value v;
    memset(&v, 0, sizeof(v));
    v.type = T_INT;
    v.u.i = i;
    return v;
}

/* 算术要求左右均为 T_INT；否则报错并返回 false */
static bool require_int(const Value *l, const Value *r)
{
    if (l->type != T_INT || r->type != T_INT) {
        ereport(SLDB_ERR_TYPE, "arithmetic requires integer operands, got %d and %d",
                l->type, r->type);
        return false;
    }
    return true;
}

/* 把 Value 解释成布尔：T_BOOL 直接取；T_INT 非 0 为真；其余报错 */
static bool value_as_bool(const Value *v)
{
    if (v->isnull) ereport(SLDB_ERR_TYPE, "NULL in logical expression");
    if (v->type == T_BOOL) return v->u.b;
    if (v->type == T_INT)  return v->u.i != 0;
    ereport(SLDB_ERR_TYPE, "logical operand must be boolean/integer, got %d", v->type);
    return false;
}

/* ---------------- 算术 / 比较分派（按 oper 显式 switch） ---------------- */
static Value eval_arith_cmp(int oper, const Value *l, const Value *r)
{
    switch (oper) {
    /* ---------------- 算术 ---------------- */
    case '+':
        if (!require_int(l, r)) return (Value){ .type = T_INVALID, .isnull = true };
        return make_int(l->u.i + r->u.i);
    case '-':
        if (!require_int(l, r)) return (Value){ .type = T_INVALID, .isnull = true };
        return make_int(l->u.i - r->u.i);
    case '*':
        if (!require_int(l, r)) return (Value){ .type = T_INVALID, .isnull = true };
        return make_int(l->u.i * r->u.i);
    case '/':
        if (!require_int(l, r)) return (Value){ .type = T_INVALID, .isnull = true };
        if (r->u.i == 0) {
            ereport(SLDB_ERR_TYPE, "division by zero");
            return (Value){ .type = T_INVALID, .isnull = true };
        }
        return make_int(l->u.i / r->u.i);   /* 整数除（向零截断） */

    /* ---------------- 比较（结果统一为 T_INT 的 0/1） ---------------- */
    case '=':  return make_int(value_compare(l, r) == 0);
    case '<':  return make_int(value_compare(l, r) <  0);
    case '>':  return make_int(value_compare(l, r) >  0);
    case OP_LTE: return make_int(value_compare(l, r) <= 0);
    case OP_GTE: return make_int(value_compare(l, r) >= 0);
    case OP_NEQ: return make_int(value_compare(l, r) != 0);

    default:
        ereport(SLDB_ERR_TYPE, "unsupported operator %d in eval", oper);
        return (Value){ .type = T_INVALID, .isnull = true };
    }
}

/* ---------------- 逻辑短路（AND/OR/NOT） ---------------- */
static Value eval_logical(const A_Expr *e)
{
    if (e->kind == AEXPR_NOT) {              /* NOT 的操作数在 lexpr（gram.y: makeAExpr(AEXPR_NOT, $2, NULL, 0)） */
        Value v = eval_expr(e->lexpr);
        return make_int(!value_as_bool(&v));
    }

    Value l = eval_expr(e->lexpr);
    bool  lb = value_as_bool(&l);

    if (e->kind == AEXPR_AND) {
        if (!lb) return make_int(0);         /* 短路：左假即整体假 */
        Value r = eval_expr(e->rexpr);
        return make_int(value_as_bool(&r));
    } else { /* AEXPR_OR */
        if (lb)  return make_int(1);         /* 短路：左真即整体真 */
        Value r = eval_expr(e->rexpr);
        return make_int(value_as_bool(&r));
    }
}

/* ---------------- 表达式求值入口 ---------------- */
Value eval_expr(const struct Node *expr)
{
    if (expr == NULL)
        return (Value){ .type = T_INVALID, .isnull = true };

    switch (expr->type) {
    case T_A_Const: {                 /* 常量：直接返回对应 Value */
        const A_Const *c = (const A_Const *)expr;
        Value v;
        memset(&v, 0, sizeof(v));
        v.isnull = false;
        if (c->consttype == CONST_INT) {
            v.type = T_INT;
            v.u.i  = c->val.ival;     /* primnodes.h 中 ival 为 long；LP64 下与 int64_t 同宽 */
        } else { /* CONST_STRING */
            v.type = T_TEXT;
            v.u.s  = c->val.sval;     /* 解析期 Arena 持有，直接复用指针 */
        }
        return v;
    }

    case T_A_Expr: {                  /* 标量表达式：先按 kind 分流 */
        const A_Expr *e = (const A_Expr *)expr;
        /* 逻辑运算单独走短路求值（AND/OR/NOT） */
        if (e->kind == AEXPR_AND || e->kind == AEXPR_OR || e->kind == AEXPR_NOT)
            return eval_logical(e);
        /* 算术 / 比较：统一按 oper 分派 */
        Value l = eval_expr(e->lexpr);
        Value r = eval_expr(e->rexpr);
        return eval_arith_cmp(e->oper, &l, &r);
    }

    case T_ColumnRef:                 /* 纯常量场景不支持列引用 */
        ereport(SLDB_ERR_TYPE, "column reference not supported in constant evaluation");
        return (Value){ .type = T_INVALID, .isnull = true };

    default:
        ereport(SLDB_ERR_TYPE, "unsupported expression node in eval");
        return (Value){ .type = T_INVALID, .isnull = true };
    }
}

/* ---------------- 类型检查辅助 ---------------- */
int value_compare(const Value *a, const Value *b)
{
    if (a->type != b->type)
        return (int)ereport(SLDB_ERR_TYPE, "cannot compare %d with %d", a->type, b->type);
    switch (a->type) {
    case T_INT:  return (a->u.i > b->u.i) - (a->u.i < b->u.i);
    case T_TEXT: return strcmp(a->u.s, b->u.s);
    case T_BOOL: return (a->u.b > b->u.b) - (a->u.b < b->u.b);
    default:     return (int)ereport(SLDB_ERR_TYPE, "unsupported compare type");
    }
}

/* 值 → 字符串（结果打印；串由 Arena 分配或字符串字面量，调用方不 free） */
char *value_to_string(const Value *v)
{
    if (v->isnull) return "<NULL>";
    switch (v->type) {
    case T_INT: {
        char *buf = (char *)arena_alloc(current_arena, 32);
        snprintf(buf, 32, "%lld", (long long)v->u.i);
        return buf;
    }
    case T_TEXT: return v->u.s;
    case T_BOOL: return v->u.b ? "true" : "false";
    default:     return "<?>";
    }
}
