/*
 * error.c - 错误状态实现（对标 PostgreSQL elog.c 的简化版）
 */
#include "error.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

SldbError sldb_errcode = SLDB_OK;
char      sldb_errmsg[512] = {0};

SldbError ereport(SldbError code, const char *fmt, ...)
{
    sldb_errcode = code;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(sldb_errmsg, sizeof(sldb_errmsg), fmt, ap);
    va_end(ap);
    return code;
}

void sldb_reset_error(void)
{
    sldb_errcode = SLDB_OK;
    sldb_errmsg[0] = '\0';
}

const char *sldb_errmsg_str(void)
{
    return sldb_errmsg;
}
