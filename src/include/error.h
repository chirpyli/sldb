/*
 * error.h - 统一错误码与错误报告（对标 PostgreSQL utils/elog.h）
 *
 * PostgreSQL 用 ereport(ERROR, (errmsg(...))) + PG_TRY/PG_CATCH（setjmp/longjmp）
 * 做错误展开与栈回退。sldb 处于单连接阶段，采用更直观的「返回错误码 +
 * 全局错误状态」风格：调用处拿到 SldbError 即可判断，无需异常机制。
 * 并发阶段若需跨层回滚，可升级为 PG_TRY/PG_CATCH 风格。
 */
#ifndef SLDB_ERROR_H
#define SLDB_ERROR_H

#include <stdarg.h>

/* 错误码（仅首版需要的分类；对标 PG 的 SQLSTATE 大类） */
typedef enum SldbError {
    SLDB_OK = 0,
    SLDB_ERR_SYNTAX,   /* 语法/解析错误        ~ ERRCODE_SYNTAX_ERROR */
    SLDB_ERR_CATALOG,  /* 表/列不存在、重复建表 ~ ERRCODE_UNDEFINED_TABLE */
    SLDB_ERR_TYPE,     /* 类型不匹配           ~ ERRCODE_DATATYPE_MISMATCH */
    SLDB_ERR_IO,       /* 文件/磁盘错误        ~ ERRCODE_IO_ERROR */
    SLDB_ERR_TXN,      /* 事务错误             ~ ERRCODE_T_R_SERIALIZATION_FAILURE */
    SLDB_ERR_INTERNAL, /* 内部/未预期错误      ~ ERRCODE_INTERNAL_ERROR */
} SldbError;

/* 全局错误状态（单连接阶段单例；并发阶段改为线程局部 __thread） */
extern SldbError sldb_errcode;
extern char      sldb_errmsg[512];

/*
 * 简化版 ereport：记录错误码 + 格式化消息，返回 code 供上层判断。
 * 用法：return ereport(SLDB_ERR_CATALOG, "table %s not found", name);
 */
SldbError ereport(SldbError code, const char *fmt, ...);

/* 每条语句执行前调用，清空错误状态 */
void sldb_reset_error(void);

/* 取最近一次错误描述（供打印） */
const char *sldb_errmsg_str(void);

#endif /* SLDB_ERROR_H */
