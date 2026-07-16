#ifndef SLDB_TYPES_H
#define SLDB_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "nodes.h"

/*
 * types.h - 数据库值的统一表示与类型系统（对标 PostgreSQL Datum / pg_type）
 *
 * 设计要点（呼应"类型系统不依赖系统表"的分析）：
 *   - DataType 是内建类型的枚举 ID；类型真源是本文件写死的 BuiltinType[]，
 *     而非磁盘上的系统表（pg_type）。系统表反过来引用这里的 DataType。
 *   - Value 是"带标签的值"：学习期比 PG 的裸 Datum 更直观；
 *     后续可改为裸 Datum + 外部类型信息（见设计文档 §10 学习建议）。
 *   - T_TEXT 的字符串指针由 Arena 分配（解析期持有），Value 仅引用，不拥有。
 */

/* 内建类型枚举（对标 PG 的 OID 常量；首版用枚举省去代码生成） */
typedef enum DataType {
    T_INVALID = 0,   /* 非法/未定义 */
    T_INT,           /* 整数（64 位有符号）  ~ INT8OID / 学习期用 int64 简化 */
    T_TEXT,          /* 变长字符串（Arena 分配）~ TEXTOID */
    T_BOOL,          /* 布尔（比较/逻辑结果）  ~ BOOLOID；Phase 1 求值不产生，留待 Phase 5 */
} DataType;

/* 带标签的值（对标 PG 的 Datum + 类型）：学习期直观 */
typedef struct Value {
    DataType  type;     /* 值类型 */
    bool      isnull;   /* 是否为 NULL（Phase 1 恒为 false；Phase 4 Tuple 行内 NULL 才置位） */
    union {
        int64_t  i;     /* T_INT */
        char    *s;     /* T_TEXT（Arena 分配，变长） */
        bool     b;     /* T_BOOL */
    } u;
} Value;

/*
 * 内建类型表：C 层写死，是类型系统的真源。
 * 对标 PostgreSQL src/backend/catalog/pg_type.c 的 bootstrappg_type。
 * pg_type 系统表后续由它派生填充，而不是反过来。
 */
typedef struct BuiltinType {
    const char *name;   /* 类型名（小写，如 "int"/"text"） */
    DataType    type;   /* 对应枚举 */
    int16_t     attlen; /* 内部长度；-1 表示变长（TEXT） */
} BuiltinType;

/* 前向声明：Tuple 属 Phase 4（access.h），本阶段仅常量求值，不引用其定义。
 * Phase 4 将把 eval_expr 升级为 Value eval_expr(const Node*, const Tuple*)。 */
typedef struct Tuple Tuple;

/* 前向声明：TypeName 在 primnodes.h 完整定义；此处仅以指针形式引用，
 * 保持 types.h 作为无依赖叶子节点（不引入 primnodes.h）。 */
struct TypeName;

/* 类型名 → DataType（查内置类型表；找不到返回 T_INVALID） */
DataType typename_to_datatype(const struct TypeName *tn);

/* 常量表达式求值：返回带标签 Value（纯常量场景，无行上下文） */
Value eval_expr(const struct Node *expr);

/* 两值比较：相等返回 0，a<b 返回负数，a>b 返回正数；类型不匹配报错 */
int value_compare(const Value *a, const Value *b);

/* 值 → 字符串（结果打印用；返回的串由 Arena 分配或字符串字面量，调用方不 free） */
char *value_to_string(const Value *v);

#endif /* SLDB_TYPES_H */
