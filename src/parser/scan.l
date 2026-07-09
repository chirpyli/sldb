%{
/*
 * scan.l - SLDB SQL 词法分析器（flex）
 * 参考 PostgreSQL src/backend/parser/scan.l
 */
#include "gram.h"
#include "mem.h"
#include "scansup.h"
#include <stdlib.h>
#include <string.h>
%}

%option noyywrap
%option nounput
%option noinput

%%

[ \t\r\n]+              { /* 跳过空白 */ }

"--"[^\n]*              { /* 跳过单行注释 */ }

"/*"([^*]|\*+[^*/])*\*+"/"   { /* 跳过块注释 */ }

"="     { return '='; }
"<"     { return '<'; }
">"     { return '>'; }
"<="    { return OP_LTE; }
">="    { return OP_GTE; }
"<>"    { return OP_NEQ; }
"!="    { return OP_NEQ; }
","     { return ','; }
"("     { return '('; }
")"     { return ')'; }
";"     { return ';'; }
"+"     { return '+'; }
"-"     { return '-'; }
"*"     { return '*'; }
"/"     { return '/'; }

[0-9]+                  { yylval.ival = atol(yytext); return ICONST; }

'(''|[^'])*'            {
        yylval.str = strip_string_literal(yytext);
        return SCONST;
    }

\"[^\"]*\"              {
        yylval.str = strip_quoted_identifier(yytext);
        return IDENT;
    }

[A-Za-z_][A-Za-z0-9_]*  {
        int kw = scan_keyword(yytext);
        if (kw >= 0) return kw;
        yylval.str = arena_strdup(current_arena, yytext);
        return IDENT;
    }

.                       { return yytext[0]; }

%%
