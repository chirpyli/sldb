#ifndef PARSER_H
#define PARSER_H

#include "pg_list.h"

/* 解析状态（在 gram.y 与 main.c 之间共享） */
typedef struct ParserState {
    List *parse_tree;     /* RawStmt 列表 */
    char  err_msg[512];
    int   error;
} ParserState;

/* classic bison 模式下的全局当前解析状态 */
extern ParserState *g_parser_state;

/* 由 flex 生成，bison 调用 */
int yylex(void);

/* 由 bison 在语法错误时调用 */
void yyerror(const char *msg);

#endif
