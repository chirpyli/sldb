#ifndef _GNU_SOURCE   /* 启用 fmemopen / getline / strndup（glibc 扩展）；Makefile 已通过 -D_GNU_SOURCE 定义 */
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     /* isatty */
#include <ctype.h>      /* isspace */
#include <sys/stat.h>   /* mkdir */
#include <sys/types.h>
#include "mem.h"
#include "nodes.h"
#include "pg_list.h"
#include "primnodes.h"
#include "parsenodes.h"
#include "parser.h"
#include "gram.h"
#include "error.h"
#include "types.h"       /* Phase 1：eval_expr / value_to_string / Value */
#include "repl.h"

/* flex 提供的输入文件指针（classic 模式全局） */
extern FILE *yyin;
/* flex 生成的缓冲重置函数：切换到新的输入流并丢弃旧缓冲 */
extern void yyrestart(FILE *input_file);

/* ===================== 以下为 AST 打印（沿用解析层 print_node） ===================== */

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

/* ===================== 语句派发骨架（对标 exec_simple_query 的命令路由） ===================== */
/*
 * dispatch_stmt - 按 NodeTag 把语句路由到未来的各 Phase 实现。
 * 本阶段所有分支仅打印 AST（保留现有行为），用 TODO 标明未来接入点。
 */
static void dispatch_stmt(RawStmt *rs)
{
    Node *stmt = rs->stmt;
    switch (stmt->type) {
    case T_CreateStmt:
        /* TODO Phase 3：catalog_create_table(...) 真正登记元数据 + 建堆文件 */
        printf("[DDL] "); print_node(stmt, 0); break;
    case T_DropStmt:
        /* TODO Phase 3：catalog_drop_table(...) 删除元数据 + 删堆文件 */
        printf("[DDL] "); print_node(stmt, 0); break;
    case T_SelectStmt: {
        SelectStmt *s = (SelectStmt *)stmt;
        /* Phase 1：无 FROM 的纯常量 SELECT 直接求值并打印（端到端验证类型系统，
         * 印证"类型系统可脱离系统表独立工作"——见设计文档 §8.4）。 */
        if (s->fromClause == NULL || list_length(s->fromClause) == 0) {
            printf("[EVAL] ");
            for (int i = 0; i < list_length(s->targetList); i++) {
                ResTarget *rt = (ResTarget *)list_nth(s->targetList, i);
                if (rt->val == NULL) continue;
                Value v = eval_expr(rt->val);
                if (sldb_errcode != SLDB_OK) {    /* 除零 / 类型不匹配等 */
                    fprintf(stderr, "Eval error: %s\n", sldb_errmsg_str());
                    sldb_reset_error();
                    break;
                }
                if (i) printf(" | ");
                printf("%s", value_to_string(&v));
            }
            break;
        }
        print_node(stmt, 0);   /* 含 FROM 的查询留待后续 Phase 接入执行器 */
        break;
    }
    case T_InsertStmt:  /* TODO Phase 4：Executor Insert 节点 */
    case T_UpdateStmt:  /* TODO Phase 4 */
    case T_DeleteStmt:  /* TODO Phase 4 */
    default:
        print_node(stmt, 0); break;
    }
    printf("\n");
}

/* ===================== 核心：解析 + 派发（对标 exec_simple_query 的解析阶段） ===================== */
/*
 * repl_process_sql - 解析一条 SQL 文本并派发执行。
 * 用内存 FILE* 喂给 classic flex 扫描器（fmemopen + yyrestart），零改动 scan.l/gram.y。
 */
void repl_process_sql(const char *sql)
{
    sldb_reset_error();

    /* 每条语句用独立 Arena，打印完 AST 即释放；避免 REPL 长会话内存膨胀 */
    Arena *arena = arena_create();
    set_current_arena(arena);

    ParserState pstate;
    memset(&pstate, 0, sizeof(pstate));
    g_parser_state = &pstate;          /* 解析结果写入该状态（parser.h） */

    /* 用内存 FILE* 喂给 classic flex 扫描器，零改动 scan.l/gram.y */
    FILE *f = fmemopen((char *)sql, strlen(sql), "r");
    if (!f) {
        fprintf(stderr, "internal error: fmemopen failed\n");
        arena_destroy(arena);
        return;
    }
    yyin = f;
    yyrestart(f);                      /* 关键：重置 flex 输入缓冲，避免读到上条残留 */
    int r = yyparse();
    fclose(f);

    if (r != 0 || pstate.error) {
        fprintf(stderr, "Parse error: %s\n", pstate.err_msg);
    } else if (pstate.parse_tree) {
        for (int i = 0; i < list_length(pstate.parse_tree); i++) {
            RawStmt *rs = (RawStmt *)list_nth(pstate.parse_tree, i);
            dispatch_stmt(rs);         /* 本阶段打印 AST；后续 Phase 接执行器 */
        }
    }
    arena_destroy(arena);              /* AST 已在本函数内打印，可安全释放 */
}

/* ===================== 输入源实现：stdin（交互 REPL） ===================== */

typedef struct {
    char  *buf;     /* 累积尚未执行的缓冲 */
    size_t len;
    size_t cap;
} StdinCtx;

/* 把 s 的前 n 字节追加到累积缓冲（可增长） */
static void stdin_append(StdinCtx *c, const char *s, size_t n)
{
    if (c->len + n + 1 > c->cap) {
        size_t newcap = c->cap ? c->cap : 256;
        while (newcap < c->len + n + 1) newcap *= 2;
        c->buf = realloc(c->buf, newcap);
        c->cap = newcap;
    }
    memcpy(c->buf + c->len, s, n);
    c->len += n;
    c->buf[c->len] = '\0';
}

/* 读下一条完整 SQL（到 ';' 结束），动态分配返回；EOF 返回 NULL。
 * 仅在缓冲为空时打印提示符，并把 ';' 之后的剩余内容留在缓冲供下次读取。 */
static char *stdin_read_stmt(SldbInputSource *src)
{
    StdinCtx *c = (StdinCtx *)src->ctx;
    char *line = NULL;
    size_t n = 0;
    for (;;) {
        /* 仅在交互模式（stdin 是终端）打印提示符，避免管道批处理时输出噪声 */
        if (c->len == 0 && isatty(fileno(stdin))) { printf("sldb> "); fflush(stdout); }
        ssize_t got = getline(&line, &n, stdin);
        if (got < 0) {
            free(line);
            /* EOF：若缓冲中仍有内容（可能不含换行的最后一条语句），作为最后一条返回 */
            if (c->len > 0) {
                char *out = strndup(c->buf, c->len);
                c->len = 0;
                c->buf[0] = '\0';
                return out;
            }
            return NULL;   /* 真正无更多输入（Ctrl-D） */
        }

        /* 元命令：当前缓冲无实质内容（仅空白）时，'\q' 表示退出 */
        if (strncmp(line, "\\q", 2) == 0) {
            int only_ws = 1;
            for (size_t i = 0; i < c->len; i++)
                if (!isspace((unsigned char)c->buf[i])) { only_ws = 0; break; }
            if (only_ws) { free(line); return NULL; }
        }

        stdin_append(c, line, (size_t)got);

        char *semi = strchr(c->buf, ';');
        if (semi) {
            size_t stmt_len = (size_t)(semi - c->buf) + 1;  /* 含 ';' */
            char *out = strndup(c->buf, stmt_len);
            /* 保留 ';' 之后的剩余内容 */
            memmove(c->buf, c->buf + stmt_len, c->len - stmt_len);
            c->len -= stmt_len;
            c->buf[c->len] = '\0';
            free(line);
            return out;
        }
    }
}

static void stdin_close(SldbInputSource *src)
{
    StdinCtx *c = (StdinCtx *)src->ctx;
    free(c->buf);
    free(c);
    free(src);
}

SldbInputSource *repl_stdin_source(void)
{
    SldbInputSource *src = malloc(sizeof(SldbInputSource));
    StdinCtx *c = malloc(sizeof(StdinCtx));
    c->buf = NULL; c->len = 0; c->cap = 0;
    src->ctx = c;
    src->read_stmt = stdin_read_stmt;
    src->close = stdin_close;
    return src;
}

/* ===================== 输入源实现：file（批处理） ===================== */

typedef struct {
    char   *text;   /* 整个文件内容 */
    size_t  pos;    /* 当前读取偏移 */
    size_t  total;  /* 文件总长度 */
} FileCtx;

/* 读下一条完整 SQL（按 ';' 切分），动态分配返回；无更多返回 NULL。
 * 注意：首版不处理字符串常量内的 ';'（学习期可接受，Phase 7 测试时规避）。 */
static char *file_read_stmt(SldbInputSource *src)
{
    FileCtx *c = (FileCtx *)src->ctx;
    if (c->pos >= c->total) return NULL;
    char *semi = strchr(c->text + c->pos, ';');
    if (!semi) {
        /* 末尾剩余内容（可能无 ';'），原样返回交由解析器判断 */
        size_t len = c->total - c->pos;
        char *out = strndup(c->text + c->pos, len);
        c->pos = c->total;
        return out;
    }
    size_t stmt_len = (size_t)(semi - (c->text + c->pos)) + 1;  /* 含 ';' */
    char *out = strndup(c->text + c->pos, stmt_len);
    c->pos += stmt_len;
    return out;
}

static void file_close(SldbInputSource *src)
{
    FileCtx *c = (FileCtx *)src->ctx;
    free(c->text);
    free(c);
    free(src);
}

SldbInputSource *repl_file_source(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); exit(1); }

    /* 读取整个文件到内存 */
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    size_t rd = fread(text, 1, (size_t)sz, fp);
    text[rd] = '\0';
    fclose(fp);

    FileCtx *c = malloc(sizeof(FileCtx));
    c->text = text; c->total = rd; c->pos = 0;

    SldbInputSource *src = malloc(sizeof(SldbInputSource));
    src->ctx = c;
    src->read_stmt = file_read_stmt;
    src->close = file_close;
    return src;
}

/* ===================== 数据库目录（对标 PGDATA） ===================== */

/* 
 * 创建 data/<db>/ 目录；已存在则忽略（EEXIST 属正常）
 * 暂不实现数据库OID，仅建目录
 */
static void db_init(const char *dbname)
{
    mkdir("data", 0755);
    char path[256];
    snprintf(path, sizeof(path), "data/%s", dbname);
    mkdir(path, 0755);
}

/* ===================== 入口 ===================== */

int main(int argc, char **argv)
{
    db_init("main");   /* 创建 data/main/（Phase 0 仅建目录，Phase 2/3 才写文件） */

    SldbInputSource *src = (argc > 1)
        ? repl_file_source(argv[1])    /* 批处理 */
        : repl_stdin_source();         /* 交互 REPL */

    char *sql;
    while ((sql = src->read_stmt(src)) != NULL) {
        repl_process_sql(sql);
        free(sql);
    }
    src->close(src);
    return 0;
}
