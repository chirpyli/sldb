#ifndef SCANSUP_H
#define SCANSUP_H

/* 大小写不敏感的关键字查找 */
int  scan_keyword(const char *text);

/* 去除字符串字面量首尾引号，处理 '' 转义 */
char *strip_string_literal(const char *text);

/* 去除带引号标识符首尾双引号 */
char *strip_quoted_identifier(const char *text);

#endif
