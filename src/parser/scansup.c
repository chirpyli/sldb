#include "scansup.h"
#include "gram.h"
#include "mem.h"
#include <strings.h>
#include <string.h>

/*
 * scan_keyword - 关键字查找（大小写不敏感）
 *
 * 参考 PostgreSQL ScanKeywordLookup()。当前为线性查找，
 * 关键字增多时可改为 hash 表或二分查找。
 */
int scan_keyword(const char *text) {
    if (strcasecmp(text, "select") == 0)  return KW_SELECT;
    if (strcasecmp(text, "from") == 0)    return KW_FROM;
    if (strcasecmp(text, "where") == 0)   return KW_WHERE;
    if (strcasecmp(text, "insert") == 0)  return KW_INSERT;
    if (strcasecmp(text, "into") == 0)    return KW_INTO;
    if (strcasecmp(text, "values") == 0)  return KW_VALUES;
    if (strcasecmp(text, "update") == 0)  return KW_UPDATE;
    if (strcasecmp(text, "set") == 0)     return KW_SET;
    if (strcasecmp(text, "delete") == 0)  return KW_DELETE;
    if (strcasecmp(text, "create") == 0)  return KW_CREATE;
    if (strcasecmp(text, "drop") == 0)    return KW_DROP;
    if (strcasecmp(text, "table") == 0)   return KW_TABLE;
    if (strcasecmp(text, "order") == 0)   return KW_ORDER;
    if (strcasecmp(text, "by") == 0)      return KW_BY;
    if (strcasecmp(text, "asc") == 0)     return KW_ASC;
    if (strcasecmp(text, "desc") == 0)    return KW_DESC;
    if (strcasecmp(text, "limit") == 0)   return KW_LIMIT;
    if (strcasecmp(text, "as") == 0)      return KW_AS;
    if (strcasecmp(text, "and") == 0)     return KW_AND;
    if (strcasecmp(text, "or") == 0)      return KW_OR;
    if (strcasecmp(text, "not") == 0)     return KW_NOT;
    if (strcasecmp(text, "null") == 0)    return KW_NULL;
    if (strcasecmp(text, "int") == 0)     return KW_INT;
    if (strcasecmp(text, "integer") == 0) return KW_INT;
    if (strcasecmp(text, "text") == 0)    return KW_TEXT;
    if (strcasecmp(text, "varchar") == 0) return KW_VARCHAR;
    return -1;
}

/* 'it''s' -> it's */
char *strip_string_literal(const char *text) {
    size_t len = strlen(text);
    char *buf = (char *)arena_alloc(current_arena, len);  /* 至多 len-2 + 1 */
    size_t j = 0;
    for (size_t i = 1; i + 1 < len; i++) {
        if (text[i] == '\'' && i + 2 < len && text[i + 1] == '\'') {
            buf[j++] = '\'';
            i++;          /* 跳过被转义的那个引号 */
        } else {
            buf[j++] = text[i];
        }
    }
    buf[j] = '\0';
    return buf;
}

char *strip_quoted_identifier(const char *text) {
    size_t len = strlen(text);
    char *buf = (char *)arena_alloc(current_arena, len);
    size_t j = 0;
    for (size_t i = 1; i + 1 < len; i++)
        buf[j++] = text[i];
    buf[j] = '\0';
    return buf;
}
