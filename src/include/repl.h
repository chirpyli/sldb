/*
 * repl.h - 输入源抽象与 REPL 入口
 *
 * 对标 PostgreSQL：客户端（psql/libpq）与服务端（PostgresMain）之间由网络协议
 * 隔离。sldb 嵌入式形态下二者同进程，但我们仍把「下一条完整 SQL 从哪来」
 * 抽象为 SldbInputSource，未来接 C/S 时只需新增一个 read_stmt 实现
 * （从 socket 读消息），上层派发逻辑完全不动。
 */
#ifndef SLDB_REPL_H
#define SLDB_REPL_H

/* 输入源抽象：屏蔽「下一条完整 SQL 从 stdin 还是文件/网络来」的差异 */
typedef struct SldbInputSource {
    void *ctx;                                       /* 实现私有数据 */
    /* 读取下一条完整 SQL（累积到 ';' 结束），动态分配返回；无更多返回 NULL */
    char *(*read_stmt)(struct SldbInputSource *src);
    void  (*close)(struct SldbInputSource *src);
} SldbInputSource;

/* 构造输入源 */
SldbInputSource *repl_stdin_source(void);    /* 交互式 REPL（sldb> 提示符） */
SldbInputSource *repl_file_source(const char *path); /* 批处理（sldb file.sql） */

/* 处理一条 SQL 文本：解析 + 派发（核心循环调用） */
void repl_process_sql(const char *sql);

#endif /* SLDB_REPL_H */
