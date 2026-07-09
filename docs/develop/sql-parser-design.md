# SQL 解析器技术选型与设计方案

> 方案：C 语言 + flex（词法分析）+ bison（语法分析），参考 PostgreSQL 解析器架构

## 0. 实现状态

本方案已全部实现并通过测试。代码位于 `src/`，构建与运行方式：

```bash
make              # 构建 sldb 可执行文件
make test         # 运行 test/sql/ 下全部用例
./sldb test/sql/select_basic.sql   # 解析单个文件
echo "SELECT 1;" | ./sldb          # 从标准输入解析
```

已实现能力：SELECT（含 WHERE / ORDER BY / LIMIT / 别名 / `*` /
算术与逻辑表达式 / 括号）、INSERT（带列名或不带）、UPDATE（多列 SET）、
DELETE、CREATE TABLE、DROP TABLE；大小写不敏感；支持 `/* */` 与 `--` 注释；
语法错误以 `Parse error: ...` 形式报告。

实现要点与文档设计保持一致：`scan.l`（flex）对应 PostgreSQL `scan.l`，
`gram.y`（bison LALR(1)）对应 `gram.y`，`nodes.h` 的 `NodeTag` 系统、
`pg_list.h` 的 `List`、Arena 分配器分别对应 PostgreSQL 的节点系统与
MemoryContext。

---

## 1. 需求概述

本项目为学习型关系数据库 sldb，解析器阶段需支持以下 SQL 语句：

| 语句类型     | 示例                                                                    |
| ------------ | ----------------------------------------------------------------------- |
| SELECT       | `SELECT id, name FROM users WHERE age > 18 ORDER BY id DESC LIMIT 10` |
| INSERT       | `INSERT INTO users (id, name) VALUES (1, 'Alice')`                    |
| UPDATE       | `UPDATE users SET name = 'Bob' WHERE id = 1`                          |
| DELETE       | `DELETE FROM users WHERE id = 1`                                      |
| CREATE TABLE | `CREATE TABLE users (id INT, name TEXT, age INT)`                     |
| DROP TABLE   | `DROP TABLE users`                                                    |

需要支持的基本数据类型：整数（INT）、字符串（TEXT/VARCHAR）。

---

## 2. 为什么选择 C + flex + bison？

### 2.1 与 PostgreSQL 的对标

PostgreSQL 是全球最先进的开源关系数据库之一，其 SQL 解析器架构在工业界经过 30+ 年验证：

| 组件         | PostgreSQL 实现                               | sldb 对标        |
| ------------ | --------------------------------------------- | ---------------- |
| 词法分析     | `src/backend/parser/scan.l` (flex)          | `scan.l`       |
| 语法分析     | `src/backend/parser/gram.y` (bison)         | `gram.y`       |
| AST 节点系统 | `src/include/nodes/nodes.h`                 | `nodes.h`      |
| AST 节点定义 | `src/include/nodes/parsenodes.h`            | `parsenodes.h` |
| 内存管理     | MemoryContext (palloc/pfree)                  | 简化版 Arena     |
| 节点工具     | `src/backend/nodes/` (copyFuncs/equalfuncs) | 简化版           |

采用与 PostgreSQL 相同的架构，意味着：

- 可以直接阅读 PostgreSQL 源码来学习更高级的解析技巧
- 延伸学习路径清晰：从 sldb 的简化实现平滑过渡到理解 PostgreSQL 的复杂实现
- 语法文件（.l / .y）本身就是优秀的学习文档

### 2.2 C 语言的选择理由

| 维度                                | 分析                                                             |
| ----------------------------------- | ---------------------------------------------------------------- |
| **与 PostgreSQL/SQLite 一致** | 主流数据库几乎都用 C，学习成果可直接迁移到阅读这些项目的源码     |
| **底层控制**                  | 手动内存管理对于理解数据库 Buffer Pool、WAL 等存储层机制至关重要 |
| **工具链成熟**                | flex/bison 是 C 生态标准工具，40+ 年历史，稳定可靠               |
| **学习深度**                  | 从内存分配到语法分析，每一层都透明，不存在"黑盒"                 |
| **性能**                      | C 编译为原生代码，零运行时开销                                   |

### 2.3 flex + bison 的选择理由

| 对比维度          | 手写递归下降       | flex + bison（本方案）              |
| ----------------- | ------------------ | ----------------------------------- |
| 学习价值          | 理解解析器编写技巧 | 理解形式化语法、自动机理论、LALR(1) |
| 语法描述          | 代码中隐含语法规则 | `.y` 文件显式 BNF 语法，即文档    |
| 左递归处理        | 需手动消除         | bison 原生处理左递归                |
| 冲突检测          | 手动发现           | bison 自动报告 shift/reduce 冲突    |
| 运算符优先级      | 手写 Pratt Parsing | `%left`/`%right` 一行声明       |
| 错误恢复          | 手动编码           | bison 提供`error` token 支持      |
| 词法分析          | 手写状态机         | flex 生成高效 DFA                   |
| PostgreSQL 一致性 | 不一致             | **完全一致**                  |

flex + bison 是本场景的最优解：既保留了深入理解解析原理的学习价值（需理解 LR 语法和自动机），又避免了手写词法状态机和手动处理左递归的机械劳动。

---

## 3. 整体架构

```
                           ┌──────────────────────────┐
                           │     SQL 文本输入            │
                           │  "SELECT id FROM t WHERE  │
                           │   id > 1;"                │
                           └────────────┬─────────────┘
                                        │
                                        ▼
┌───────────────────────────────────────────────────────────────────┐
│                     flex 词法分析器 (scan.l)                        │
│                                                                    │
│  ┌──────────────┐    ┌───────────────┐    ┌──────────────────┐   │
│  │ 关键字匹配     │    │ 标识符/数字     │    │ 运算符/分隔符      │   │
│  │ SELECT → KW_  │    │  /字符串识别   │    │ = → '=', ( → '(' │   │
│  │ SELECT        │    │               │    │                  │   │
│  └──────────────┘    └───────────────┘    └──────────────────┘   │
│                                                                    │
│  输出: Token 序列:  KW_SELECT IDENT("id") KW_FROM IDENT("t") ...  │
└────────────────────────────────┬──────────────────────────────────┘
                                 │
                                 ▼
┌───────────────────────────────────────────────────────────────────┐
│                     bison 语法分析器 (gram.y)                       │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │  BNF 语法规则                                                  │ │
│  │  stmt: SelectStmt | InsertStmt | UpdateStmt | ...             │ │
│  │  SelectStmt: KW_SELECT target_list KW_FROM relation_expr ...  │ │
│  │  a_expr: a_expr '+' a_expr  { $$ = makeBinaryExpr($1,$2,$3);}│ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  输出: AST (raw_parse_tree)                                       │
└────────────────────────────────┬──────────────────────────────────┘
                                 │
                                 ▼
┌───────────────────────────────────────────────────────────────────┐
│                    AST 节点树                                       │
│                                                                    │
│  SelectStmt Node                                                   │
│  ├── type: T_SelectStmt                                            │
│  ├── targetList: List ─── [ResTarget("id"), ResTarget("name")]    │
│  ├── fromClause: List ─── [RangeVar("users")]                     │
│  ├── whereClause: A_Expr                                          │
│  │   ├── type: T_A_Expr                                            │
│  │   ├── kind: AEXPR_OP                                            │
│  │   ├── lexpr: ColumnRef("age")                                   │
│  │   ├── oper: Operator(">")                                       │
│  │   └── rexpr: Integer(18)                                        │
│  ├── sortClause: List ─── [SortBy(...)]                            │
│  └── limitCount: Integer(10)                                       │
└───────────────────────────────────────────────────────────────────┘
```

---

## 4. 项目结构

```
sldb/
├── Makefile                    # 顶层构建文件
├── src/
│   ├── main.c                  # 入口：读取 SQL → 解析 → 打印 AST
│   │
│   ├── include/
│   │   ├── nodes.h             # Node 基类型、NodeTag 枚举
│   │   ├── parsenodes.h        # 各语句 AST 节点结构体定义
│   │   ├── primnodes.h         # 表达式/运算符等基础节点
│   │   ├── pg_list.h           # List 链表实现（参考 PostgreSQL）
│   │   └── mem.h               # Arena 内存分配器
│   │
│   └── parser/
│       ├── scan.l              # flex 词法分析规则
│       ├── gram.y              # bison 语法分析规则
│       ├── kwlist.h            # 关键字查找表
│       └── scansup.c           # 词法分析辅助函数
│   │
│   ├── nodes/
│   │   ├── node.c              # makeNode / palloc 等节点分配
│   │   ├── list.c              # List 操作（lappend、lcons 等）
│   │   ├── copyfuncs.c         # 节点深拷贝
│   │   └── equalfuncs.c        # 节点相等性比较
│   │
│   └── mem/
│       └── arena.c             # 简化版 Arena 分配器
│
├── test/
│   └── sql/                    # SQL 测试用例
│       ├── select_basic.sql
│       ├── insert_basic.sql
│       ├── update_basic.sql
│       ├── delete_basic.sql
│       ├── create_table.sql
│       └── drop_table.sql
│
└── docs/
    ├── develop/
    │   └── sql-parser-design.md  # 本文档
    └── design/
```

---

## 5. AST 节点系统设计（参考 PostgreSQL nodes.h）

### 5.1 NodeTag —— 节点类型标识

PostgreSQL 使用 `NodeTag` 枚举标识每个节点的类型，这是整个节点系统的基石：

```c
/*
 * nodes.h - Node 基础类型定义
 *
 * 设计原则（来自 PostgreSQL）：
 *   1. 每个 AST 节点的第一个字段必须是 NodeTag type
 *   2. Node 是所有节点的基类型，只包含 type 字段
 *   3. 通过 NodeTag 进行运行时类型识别（类似虚函数表）
 */

/* 所有节点类型的枚举 */
typedef enum NodeTag
{
    /* 语句节点 */
    T_Invalid = 0,
    T_RawStmt,              /* 原始语句（包装器） */
    T_SelectStmt,           /* SELECT 语句 */
    T_InsertStmt,           /* INSERT 语句 */
    T_UpdateStmt,           /* UPDATE 语句 */
    T_DeleteStmt,           /* DELETE 语句 */
    T_CreateStmt,           /* CREATE TABLE 语句 */
    T_DropStmt,             /* DROP TABLE 语句 */

    /* 表达式节点 */
    T_ColumnRef,            /* 列引用: a, t.a */
    T_A_Expr,               /* 二元表达式: a + b, a > b */
    T_A_Const,              /* 常量: 1, 'hello' */
    T_TypeCast,             /* 类型转换: ::int */

    /* 辅助节点 */
    T_RangeVar,             /* 表引用: tablename */
    T_ResTarget,            /* 结果列: colname 或 expr AS alias */
    T_ColumnDef,            /* 列定义: colname type */
    T_TypeName,             /* 类型名: INT, TEXT */
    T_SortBy,               /* ORDER BY 子句 */
    T_SetClause,            /* SET col = val */
    T_InsertTarget,         /* INSERT INTO (col1, col2) */

    /* 列表 */
    T_List,
} NodeTag;

/*
 * 基节点类型：所有 AST 节点结构体的第一个字段
 * 通过将 NodeTag 放在首字段，可以安全地将任何节点指针转换
 * 为 Node* 并读取其类型
 */
typedef struct Node
{
    NodeTag type;
} Node;

/* 类型检查宏（参考 PostgreSQL IsA） */
#define IsA(nodeptr, tag)  (((const Node*)(nodeptr))->type == (tag))

/* 类型转换宏 */
#define castNode(type, nodeptr)  ((type*)(nodeptr))
```

### 5.2 语句节点定义（parsenodes.h）

```c
/*
 * RawStmt - 原始语句包装器
 * 每条解析出的 SQL 语句包装在此节点中，便于统一处理
 */
typedef struct RawStmt
{
    NodeTag type;       /* = T_RawStmt */
    Node   *stmt;       /* 指向实际语句节点 */
    int     stmt_len;   /* 原始 SQL 语句长度（字节） */
} RawStmt;

/*
 * SelectStmt - SELECT 语句
 *
 * 对应 SQL：
 *   SELECT [DISTINCT] target_list
 *     FROM from_clause
 *    WHERE where_clause
 *    ORDER BY sort_clause
 *    LIMIT limit_count
 */
typedef struct SelectStmt
{
    NodeTag type;           /* = T_SelectStmt */
    List   *targetList;     /* ResTarget 列表 */
    List   *fromClause;     /* RangeVar 列表 */
    Node   *whereClause;    /* WHERE 表达式 (A_Expr)，可为 NULL */
    List   *sortClause;     /* SortBy 列表 */
    Node   *limitCount;     /* LIMIT 值 (A_Const)，可为 NULL */
} SelectStmt;

/*
 * InsertStmt - INSERT 语句
 *
 * INSERT INTO tablename (col1, col2) VALUES (val1, val2)
 */
typedef struct InsertStmt
{
    NodeTag type;           /* = T_InsertStmt */
    RangeVar *relation;     /* 表名 */
    List     *cols;         /* 列名列表 (ResTarget) */
    List     *values;       /* 值表达式列表 (A_Expr/A_Const) */
} InsertStmt;

/*
 * UpdateStmt - UPDATE 语句
 *
 * UPDATE tablename SET col = val, ... WHERE condition
 */
typedef struct UpdateStmt
{
    NodeTag  type;          /* = T_UpdateStmt */
    RangeVar *relation;     /* 表名 */
    List     *targetList;   /* SetClause 列表 */
    Node     *whereClause;  /* WHERE 条件 */
} UpdateStmt;

/*
 * DeleteStmt - DELETE 语句
 *
 * DELETE FROM tablename WHERE condition
 */
typedef struct DeleteStmt
{
    NodeTag  type;          /* = T_DeleteStmt */
    RangeVar *relation;     /* 表名 */
    Node     *whereClause;  /* WHERE 条件 */
} DeleteStmt;

/*
 * CreateStmt - CREATE TABLE 语句
 *
 * CREATE TABLE tablename (col1 type1, col2 type2, ...)
 */
typedef struct CreateStmt
{
    NodeTag  type;          /* = T_CreateStmt */
    RangeVar *relation;     /* 表名 */
    List     *tableElts;    /* ColumnDef 列表 */
} CreateStmt;

/*
 * DropStmt - DROP TABLE 语句
 *
 * DROP TABLE tablename
 */
typedef struct DropStmt
{
    NodeTag  type;          /* = T_DropStmt */
    List    *objects;       /* 对象名列表 (RangeVar)，支持 DROP TABLE a, b */
    int      removeType;    /* OBJECT_TABLE */
} DropStmt;
```

### 5.3 表达式和基础节点定义（primnodes.h）

```c
/*
 * RangeVar - 表/关系引用
 *
 * 表示 FROM 子句中的表名，如 FROM users
 * 扩展：可支持 schema.table 形式
 */
typedef struct RangeVar
{
    NodeTag type;           /* = T_RangeVar */
    char   *relname;        /* 表名 */
    char   *schemaname;     /* schema名（可选，暂为 NULL） */
} RangeVar;

/*
 * ColumnRef - 列引用
 *
 * 表示 SQL 中的列名，如 SELECT id FROM t
 * fields 列表第一个元素为列名，可扩展为 t.id 形式
 */
typedef struct ColumnRef
{
    NodeTag type;           /* = T_ColumnRef */
    List   *fields;         /* 字段名列表 (String 节点) */
} ColumnRef;

/*
 * ResTarget - 结果目标列
 *
 * SELECT 后面的列，如 SELECT id, name AS n FROM t
 * - 如果是简单列名：name = "id"，val = ColumnRef("id")
 * - 如果带别名：name = "n"，val = ColumnRef("name")
 */
typedef struct ResTarget
{
    NodeTag type;           /* = T_ResTarget */
    char   *name;           /* 列别名（可选） */
    Node   *val;            /* 列值表达式 */
} ResTarget;

/*
 * A_Expr - 标量表达式（二元运算）
 *
 * 表示 a op b 形式的表达式，如 a > 1, a + b, id = 5
 */
typedef enum A_Expr_Kind
{
    AEXPR_OP,               /* 普通运算符：a + b, a > b */
    AEXPR_AND,              /* AND */
    AEXPR_OR,               /* OR */
    AEXPR_NOT,              /* NOT */
} A_Expr_Kind;

typedef struct A_Expr
{
    NodeTag     type;       /* = T_A_Expr */
    A_Expr_Kind kind;       /* 表达式类型 */
    Node       *lexpr;      /* 左操作数 */
    Node       *rexpr;      /* 右操作数 */
    /* 运算符名存储在 int 字段中以减少内存分配 */
    int         oper;       /* 操作符 token 值 */
} A_Expr;

/*
 * A_Const - 常量值
 */
typedef enum ConstType
{
    CONST_INT,              /* 整数 */
    CONST_STRING,           /* 字符串 */
} ConstType;

typedef struct A_Const
{
    NodeTag   type;         /* = T_A_Const */
    ConstType consttype;    /* 常量类型 */
    union
    {
        long ival;          /* CONST_INT */
        char *sval;         /* CONST_STRING */
    } val;
} A_Const;

/*
 * TypeName - 类型名
 * CREATE TABLE t (id INT, name TEXT)
 */
typedef struct TypeName
{
    NodeTag type;           /* = T_TypeName */
    char   *name;           /* 类型名: "INT", "TEXT", "VARCHAR" */
} TypeName;

/*
 * ColumnDef - 列定义
 * CREATE TABLE 中的列: colname type
 */
typedef struct ColumnDef
{
    NodeTag  type;          /* = T_ColumnDef */
    char    *colname;       /* 列名 */
    TypeName *typeName;     /* 列类型 */
} ColumnDef;

/*
 * SortBy - ORDER BY 子句
 * ORDER BY colname [ASC|DESC]
 */
typedef enum SortByDir
{
    SORTBY_DEFAULT,
    SORTBY_ASC,
    SORTBY_DESC,
} SortByDir;

typedef struct SortBy
{
    NodeTag   type;         /* = T_SortBy */
    Node     *node;         /* 排序表达式 (ColumnRef) */
    SortByDir sortby_dir;   /* 排序方向 */
} SortBy;

/*
 * SetClause - UPDATE SET 子句
 * SET col = val
 */
typedef struct SetClause
{
    NodeTag type;           /* = T_SetClause */
    char   *name;           /* 列名 */
    Node   *val;            /* 值表达式 */
} SetClause;

/*
 * String - 字符串包装节点（用于 List 中的字符串元素）
 */
typedef struct String
{
    NodeTag type;           /* 自定义 tag */
    char   *sval;
} String;
```

### 5.4 List 链表（参考 PostgreSQL pg_list.h）

```c
/*
 * List - 链表类型（参考 PostgreSQL 设计）
 *
 * List 是 PostgreSQL/SLDB 中广泛使用的数据结构。
 * 不同于标准链表，List 直接存储指针数组，支持：
 *   - 前插后插：lcons(), lappend()
 *   - 索引访问：list_nth()
 *   - 遍历：foreach() 宏
 */

typedef struct ListCell
{
    void *data;              /* 节点数据 */
} ListCell;

typedef struct List
{
    NodeTag   type;          /* = T_List */
    int       length;        /* 当前元素数量 */
    int       capacity;      /* 数组容量 */
    ListCell *elements;      /* 元素数组 */
} List;

/* --- List 操作 API --- */

/* 创建空列表 */
List *make_list(void);

/* 创建列表（预分配容量） */
List *make_list_with_capacity(int capacity);

/* 从 List 构建宏 */
#define makeList1(x1)         make_list_impl(1, (x1))
#define makeList2(x1,x2)      make_list_impl(2, (x1),(x2))

/* 添加元素 */
List *lappend(List *list, void *datum);    /* 追加到末尾 */
List *lcons(void *datum, List *list);      /* 插入到头部 */

/* 访问元素 */
void *list_nth(const List *list, int n);   /* 第 n 个元素 */

/* 遍历宏 */
#define foreach(cell, list) \
    for (int __i = 0; __i < (list)->length && ((cell) = &(list)->elements[__i]), 1; __i++)

/* 获取首个/末个元素（方便常用操作） */
#define lfirst(cell)    ((cell)->data)
#define linitial(l)     list_nth(l, 0)
#define llast(l)        list_nth(l, (l)->length - 1)

/* 列表长度 */
#define list_length(l)  ((l)->length)
```

---

## 6. flex 词法分析器设计（scan.l）

### 6.1 设计概述

参考 PostgreSQL `src/backend/parser/scan.l`，词法分析器负责：

1. 将 SQL 文本切分为 Token 序列
2. 识别关键字、标识符、数字、字符串、运算符
3. 跳过空白和注释
4. 大小写不敏感：`select`、`SELECT`、`Select` 视为同一关键字

### 6.2 LEX 符号定义（scan.l 结构）

```lex
%{
/*
 * scan.l - SLDB SQL 词法分析器
 *
 * 参考 PostgreSQL src/backend/parser/scan.l
 */

#include "nodes.h"
#include "parsenodes.h"
#include "primnodes.h"
#include "gram.h"          /* bison 生成的 token 定义 */

/* 取消 flex 默认的输入函数（使用自定义 scanner） */
#undef  YY_INPUT
#define YY_INPUT(buf, result, max_size)  scanner_yyinput(buf, &result, max_size)

/* 输入回调：在 scan.l 和 gram.y 之间共享 */
int scanner_yyinput(char *buf, int *result, int max_size);

/* 扫描器状态（由 gram.y 中的 %parse-param 传入）*/
typedef struct ScannerState
{
    const char *input;      /* 输入 SQL 文本 */
    int         pos;        /* 当前读取位置 */
    int         len;        /* 输入长度 */
} ScannerState;
%}

/* Flex 选项 */
%option noyywrap        /* 不需要 yywrap 函数 */
%option prefix="scanner_"  /* 为 yylex 添加前缀 */
%option case-insensitive   /* 大小写不敏感 */

/* ===== 正则定义 ===== */
whitespace    [ \t\n\r\f]+
digit         [0-9]
letter        [a-zA-Z_]
identifier    {letter}({letter}|{digit})*

/* 运算符 */
op_plus       "+"
op_minus      "-"
op_star       "*"
op_slash      "/"
op_eq         "="
op_lt         "<"
op_gt         ">"
op_lte        "<="
op_gte        ">="
op_neq        "<>"|"!="
op_comma      ","
op_lparen     "("
op_rparen     ")"
op_semicolon  ";"

%%

/* ===== 规则：空白与注释 ===== */
{whitespace}  { /* 跳过空白，更新行列号 */ }

/* 单行注释 -- comment */
"--"[^\n]*    { /* 跳过 */ }

/* 多行注释 */
"/*"([^*]|\*+[^*/])*\*+"/"  { /* 跳过 */ }

/* ===== 关键字匹配 ===== */
/* 
 * 使用 flex 的精确匹配：关键字优先级高于标识符
 * PostgreSQL 实现：所有关键字需要在此列出
 */

SELECT   { return KW_SELECT; }
FROM     { return KW_FROM; }
WHERE    { return KW_WHERE; }
INSERT   { return KW_INSERT; }
INTO     { return KW_INTO; }
VALUES   { return KW_VALUES; }
UPDATE   { return KW_UPDATE; }
SET      { return KW_SET; }
DELETE   { return KW_DELETE; }
CREATE   { return KW_CREATE; }
DROP     { return KW_DROP; }
TABLE    { return KW_TABLE; }
ORDER    { return KW_ORDER; }
BY       { return KW_BY; }
ASC      { return KW_ASC; }
DESC     { return KW_DESC; }
LIMIT    { return KW_LIMIT; }
AND      { return KW_AND; }
OR       { return KW_OR; }
NOT      { return KW_NOT; }
NULL     { return KW_NULL; }
INT      { return KW_INT; }
INTEGER  { return KW_INT; }
TEXT     { return KW_TEXT; }
VARCHAR  { return KW_VARCHAR; }

/* ===== 运算符 ===== */
{op_eq}       { return '='; }
{op_lt}       { return '<'; }
{op_gt}       { return '>'; }
{op_lte}      { return OP_LTE; }
{op_gte}      { return OP_GTE; }
{op_neq}      { return OP_NEQ; }
{op_comma}    { return ','; }
{op_lparen}   { return '('; }
{op_rparen}   { return ')'; }
{op_semicolon} { return ';'; }
{op_plus}     { return '+'; }
{op_minus}    { return '-'; }
{op_star}     { return '*'; }
{op_slash}    { return '/'; }

/* ===== 字面量 ===== */

/* 数字字面量 */
{digit}+      {
    yylval.ival = atol(yytext);
    return ICONST;
}

/* 字符串字面量：'hello' 或 'it''s' */
'(\\.|[^'\\])*' {
    /* 去除首尾引号，处理转义 */
    yylval.str = strip_string_literal(yytext);
    return SCONST;
}

/* 标识符 */
{identifier}  {
    /*
     * 标识符处理流程（参考 PostgreSQL）：
     * 1. 转换为小写（大小写不敏感）
     * 2. 先查关键字表 → 找到返回对应 token
     * 3. 未找到 → 返回 IDENT，yylval.str = 小写后的标识符
     */
    char *downcased = downcase_identifier(yytext);
  
    /* 查关键字表（扩展性：后续关键字多时可改用 hash 表）*/
    int kw_token = scan_keyword(downcased);
    if (kw_token >= 0)
    {
        free(downcased);
        return kw_token;
    }
  
    yylval.str = downcased;
    return IDENT;
}

/* 带引号的标识符 "MyTable"（保持原始大小写） */
\"[^\"]*\"    {
    yylval.str = strip_quoted_identifier(yytext);
    return IDENT;
}

.             { return yytext[0]; }  /* 未知字符，直接返回 */

%%

/* ===== 辅助函数 ===== */

/*
 * scan_keyword - 关键字查找
 *
 * 参考 PostgreSQL ScanKeywordLookup()
 * 当前阶段：线性查找；关键字多时可改为二分查找或 hash 表
 */
int scan_keyword(const char *text);

static char *downcase_identifier(const char *ident);
```

### 6.3 bison 与 flex 的 Token 传递

bison 的语法产生式通过 `yylval` 获取 flex 返回的语义值。bison 中通过 `%union` 定义所有可能的语义值类型：

```c
/* gram.y 中的 %union 定义 */
%union
{
    int      ival;      /* 整数值 */
    char    *str;       /* 字符串 */
    Node    *node;      /* 通用 AST 节点 */
    List    *list;      /* 节点列表 */
    SortByDir sortdir;  /* 排序方向 */
    A_Expr_Kind aexpr_kind;  /* 表达式类型 */
}
```

flex 在返回 token 前将值写入 `yylval`：

- `yylval.ival = atol(yytext); return ICONST;`  → bison 通过 `$1` 获取
- `yylval.str = strdup(...); return IDENT;`      → bison 通过 `$1` 获取

---

## 7. bison 语法分析器设计（gram.y）

### 7.1 设计概述

参考 PostgreSQL `src/backend/parser/gram.y`，bison 语法文件定义了完整的 SQL 语法规则。每条规则对应一个归约动作（action），动作用 C 代码构建对应的 AST 节点。

**核心概念**：

- **Token（终结符）**：flex 返回的 token，如 KW_SELECT、IDENT、ICONST
- **产生式（非终结符）**：语法规则的左侧，如 `stmt`、`SelectStmt`、`a_expr`
- **归约动作**：当 bison 匹配一条规则时执行的 C 代码，负责构建 AST 节点
- **$1, $2, $3**：规则右侧第 1/2/3 个符号的语义值
- **$$**：归约后向上传递的语义值

### 7.2 bison 语法文件骨架

```yacc
%{
/*
 * gram.y - SLDB SQL 语法分析器
 *
 * 参考 PostgreSQL src/backend/parser/gram.y
 */

#include "nodes.h"
#include "parsenodes.h"
#include "primnodes.h"
#include "mem.h"
#include "pg_list.h"

/* 语法分析器状态（传入 flex scanner） */
typedef struct ParserState
{
    List *parse_tree;        /* 解析结果：RawStmt 列表 */
    char *err_msg;           /* 错误信息 */
} ParserState;
%}

/* ==== 声明部分 ==== */

/* 语义值联合体类型 */
%union
{
    int          ival;
    char        *str;
    Node        *node;
    List        *list;
    SortByDir    sortdir;
    A_Expr_Kind  aexpr_kind;
    ConstType    consttype;
}

/* ==== Token 声明 ==== */

/* 关键字 */
%token KW_SELECT KW_FROM KW_WHERE KW_INSERT KW_INTO KW_VALUES
%token KW_UPDATE KW_SET KW_DELETE KW_CREATE KW_DROP KW_TABLE
%token KW_ORDER KW_BY KW_ASC KW_DESC KW_LIMIT
%token KW_AND KW_OR KW_NOT KW_NULL
%token KW_INT KW_TEXT KW_VARCHAR

/* 字面量 */
%token <str>  IDENT        /* 标识符 */
%token <ival> ICONST       /* 整数常量 */
%token <str>  SCONST       /* 字符串常量 */

/* 多字符运算符 */
%token OP_LTE OP_GTE OP_NEQ

/* ==== 类型声明（非终结符的 %union 类型） ==== */
%type <node>  stmt SelectStmt InsertStmt UpdateStmt DeleteStmt
%type <node>  CreateStmt DropStmt
%type <node>  target_el a_expr b_expr c_expr where_clause
%type <node>  opt_where_clause
%type <list>  target_list from_clause from_list
%type <list>  sort_clause opt_sort_clause
%type <list>  insert_column_list value_list
%type <list>  set_clause_list
%type <node>  set_clause sortby
%type <list>  column_def_list
%type <node>  column_def
%type <node>  type_name
%type <node>  limit_clause opt_limit_clause
%type <ival>  opt_asc_desc
%type <str>   opt_alias

/* ==== 运算符优先级与结合性 ==== */

/* 最低优先级 */
%left KW_OR
%left KW_AND
%right KW_NOT
%nonassoc '=' OP_LTE OP_GTE OP_NEQ '<' '>'   /* 比较运算符 */
%left '+' '-'
%left '*' '/'
%right UMINUS     /* 一元负号（伪 token） */
/* 最高优先级 */

/* ==== 语法规则 ==== */

%%

/*
 * 顶层规则
 *
 * stmtlist: 多条 SQL 语句的列表
 * stmt:     单条 SQL 语句（返回 RawStmt 包装器）
 */
stmtlist:
    stmt ';'
    {
        ParserState *pstate = (ParserState *)yyget_extra(yyscanner);
        pstate->parse_tree = lappend(pstate->parse_tree, $1);
    }
    | stmtlist stmt ';'
    {
        ParserState *pstate = (ParserState *)yyget_extra(yyscanner);
        pstate->parse_tree = lappend(pstate->parse_tree, $2);
    }
    ;

stmt:
    SelectStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    | InsertStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    | UpdateStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    | DeleteStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    | CreateStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    | DropStmt
    {
        RawStmt *rs = makeNode(RawStmt);
        rs->stmt = $1;
        $$ = (Node *)rs;
    }
    ;

/*
 * ============================================================
 * SELECT 语句
 * ============================================================
 *
 * BNF:
 *   SELECT target_list
 *     FROM from_clause
 *     [WHERE where_clause]
 *     [ORDER BY sort_clause]
 *     [LIMIT count]
 */

SelectStmt:
    KW_SELECT target_list
    KW_FROM from_clause
    opt_where_clause
    opt_sort_clause
    opt_limit_clause
    {
        SelectStmt *s = makeNode(SelectStmt);
        s->targetList = $2;
        s->fromClause = $4;
        s->whereClause = $5;
        s->sortClause  = $6;
        s->limitCount  = $7;
        $$ = (Node *)s;
    }
    ;

/*
 * target_list: SELECT 后的列列表
 *   SELECT id, name, age FROM t
 *          ^^^^^^^^^^^^
 */
target_list:
    target_el
    {
        $$ = makeList1($1);
    }
    | target_list ',' target_el
    {
        $$ = lappend($1, $3);
    }
    ;

/*
 * target_el: 单个结果列，支持别名
 *   SELECT id AS user_id, name FROM t
 *          ^^^^^^^^^^^^^  ^^^^
 */
target_el:
    a_expr opt_alias
    {
        ResTarget *res = makeNode(ResTarget);
        res->name = $2;     /* 别名（可为 NULL） */
        res->val  = $1;     /* 表达式 */
        $$ = (Node *)res;
    }
    | '*'
    {
        /* SELECT * ：用特殊 ColumnRef 表示 */
        ColumnRef *col = makeNode(ColumnRef);
        col->fields = makeList1(makeString("*"));
        ResTarget *res = makeNode(ResTarget);
        res->name = NULL;
        res->val  = (Node *)col;
        $$ = (Node *)res;
    }
    ;

opt_alias:
    IDENT               { $$ = $1; }
    | /* empty */       { $$ = NULL; }
    ;

/*
 * from_clause: FROM 后的表引用
 *   FROM t1, t2          → 多表
 *   FROM t1              → 单表
 */
from_clause:
    from_list               { $$ = $1; }
    ;

from_list:
    IDENT
    {
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $1;
        $$ = makeList1(rv);
    }
    | from_list ',' IDENT
    {
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        $$ = lappend($1, rv);
    }
    ;

/*
 * WHERE 子句（可选）
 */
opt_where_clause:
    KW_WHERE a_expr    { $$ = $2; }
    | /* empty */      { $$ = NULL; }
    ;

/*
 * ORDER BY 子句（可选）
 *   ORDER BY col1 ASC, col2 DESC
 */
opt_sort_clause:
    KW_ORDER KW_BY sort_clause   { $$ = $3; }
    | /* empty */                { $$ = NULL; }
    ;

sort_clause:
    sortby
    {
        $$ = makeList1($1);
    }
    | sort_clause ',' sortby
    {
        $$ = lappend($1, $3);
    }
    ;

sortby:
    a_expr opt_asc_desc
    {
        SortBy *sb = makeNode(SortBy);
        sb->node = $1;
        sb->sortby_dir = $2;
        $$ = (Node *)sb;
    }
    ;

opt_asc_desc:
    KW_ASC          { $$ = SORTBY_ASC; }
    | KW_DESC       { $$ = SORTBY_DESC; }
    | /* empty */   { $$ = SORTBY_DEFAULT; }
    ;

/*
 * LIMIT 子句（可选）
 *   LIMIT 10
 */
opt_limit_clause:
    KW_LIMIT a_expr    { $$ = $2; }
    | /* empty */      { $$ = NULL; }
    ;

/*
 * ============================================================
 * INSERT 语句
 * ============================================================
 *
 * BNF:
 *   INSERT INTO tablename [(col1, col2, ...)]
 *          VALUES (val1, val2, ...)
 */

InsertStmt:
    KW_INSERT KW_INTO IDENT insert_column_list KW_VALUES '(' value_list ')'
    {
        InsertStmt *s = makeNode(InsertStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation = rv;
        s->cols     = $4;
        s->values   = $7;
        $$ = (Node *)s;
    }
    | KW_INSERT KW_INTO IDENT KW_VALUES '(' value_list ')'
    {
        /* 不指定列名：INSERT INTO t VALUES (1, 'a') */
        InsertStmt *s = makeNode(InsertStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation = rv;
        s->cols     = NULL;    /* 不指定列名 */
        s->values   = $6;
        $$ = (Node *)s;
    }
    ;

insert_column_list:
    '(' IDENT ')'
    {
        ResTarget *res = makeNode(ResTarget);
        res->name = $2;
        res->val  = NULL;
        $$ = makeList1(res);
    }
    | insert_column_list ',' IDENT ')'
    {
        /* 处理多列的扩展语法（实际需更复杂的括号匹配）*/
        /* 此处简化，建议用独立规则处理 */
        $$ = $1;
    }
    ;

value_list:
    a_expr
    {
        $$ = makeList1($1);
    }
    | value_list ',' a_expr
    {
        $$ = lappend($1, $3);
    }
    ;

/*
 * ============================================================
 * UPDATE 语句
 * ============================================================
 *
 * BNF:
 *   UPDATE tablename SET col1 = val1, col2 = val2 WHERE condition
 */

UpdateStmt:
    KW_UPDATE IDENT KW_SET set_clause_list opt_where_clause
    {
        UpdateStmt *s = makeNode(UpdateStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $2;
        s->relation   = rv;
        s->targetList = $4;
        s->whereClause = $5;
        $$ = (Node *)s;
    }
    ;

set_clause_list:
    set_clause
    {
        $$ = makeList1($1);
    }
    | set_clause_list ',' set_clause
    {
        $$ = lappend($1, $3);
    }
    ;

set_clause:
    IDENT '=' a_expr
    {
        SetClause *sc = makeNode(SetClause);
        sc->name = $1;
        sc->val  = $3;
        $$ = (Node *)sc;
    }
    ;

/*
 * ============================================================
 * DELETE 语句
 * ============================================================
 *
 * BNF:
 *   DELETE FROM tablename WHERE condition
 */

DeleteStmt:
    KW_DELETE KW_FROM IDENT opt_where_clause
    {
        DeleteStmt *s = makeNode(DeleteStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation    = rv;
        s->whereClause = $4;
        $$ = (Node *)s;
    }
    ;

/*
 * ============================================================
 * CREATE TABLE 语句
 * ============================================================
 *
 * BNF:
 *   CREATE TABLE tablename (col1 type1, col2 type2, ...)
 */

CreateStmt:
    KW_CREATE KW_TABLE IDENT '(' column_def_list ')'
    {
        CreateStmt *s = makeNode(CreateStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->relation  = rv;
        s->tableElts = $5;
        $$ = (Node *)s;
    }
    ;

column_def_list:
    column_def
    {
        $$ = makeList1($1);
    }
    | column_def_list ',' column_def
    {
        $$ = lappend($1, $3);
    }
    ;

column_def:
    IDENT type_name
    {
        ColumnDef *cd = makeNode(ColumnDef);
        cd->colname  = $1;
        cd->typeName = (TypeName *)$2;
        $$ = (Node *)cd;
    }
    ;

type_name:
    KW_INT      { $$ = makeTypeName("INT"); }
    | KW_TEXT   { $$ = makeTypeName("TEXT"); }
    | KW_VARCHAR { $$ = makeTypeName("VARCHAR"); }
    ;

/*
 * ============================================================
 * DROP TABLE 语句
 * ============================================================
 *
 * BNF:
 *   DROP TABLE tablename
 */

DropStmt:
    KW_DROP KW_TABLE IDENT
    {
        DropStmt *s = makeNode(DropStmt);
        RangeVar *rv = makeNode(RangeVar);
        rv->relname = $3;
        s->objects    = makeList1(rv);
        s->removeType = 0;  /* OBJECT_TABLE */
        $$ = (Node *)s;
    }
    ;

/* ============================================================
 * 表达式层次结构（三层优先级）
 * ============================================================
 *
 * 参考 PostgreSQL gram.y 的表达式层次设计：
 *
 *   a_expr  → 最低优先级，OR/AND 在此层
 *   b_expr  → 中等优先级，比较运算符在此层
 *   c_expr  → 最高优先级，原子表达式（常量、列、括号）
 *
 * 这个层次 + %left/%right 声明让 bison 自动解决优先级冲突。
 */

/* c_expr: 原子表达式 */
c_expr:
    IDENT
    {
        ColumnRef *col = makeNode(ColumnRef);
        col->fields = makeList1(makeString($1));
        $$ = (Node *)col;
    }
    | ICONST
    {
        $$ = makeIntConst($1);
    }
    | SCONST
    {
        $$ = makeStringConst($1);
    }
    | KW_NULL
    {
        $$ = makeNullConst();
    }
    | '(' a_expr ')'
    {
        $$ = $2;
    }
    ;

/* b_expr: 比较表达式 */
b_expr:
    c_expr                      { $$ = $1; }
    | b_expr '+' b_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '+');
    }
    | b_expr '-' b_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '-');
    }
    | b_expr '*' b_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '*');
    }
    | b_expr '/' b_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '/');
    }
    ;

/* a_expr: 逻辑表达式 */
a_expr:
    b_expr                      { $$ = $1; }
    | a_expr '=' a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '=');
    }
    | a_expr '<' a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '<');
    }
    | a_expr '>' a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, '>');
    }
    | a_expr OP_LTE a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_LTE);
    }
    | a_expr OP_GTE a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_GTE);
    }
    | a_expr OP_NEQ a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, $1, $3, OP_NEQ);
    }
    | a_expr KW_AND a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_AND, $1, $3, 0);
    }
    | a_expr KW_OR a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_OR, $1, $3, 0);
    }
    | KW_NOT a_expr
    {
        $$ = (Node *)makeAExpr(AEXPR_NOT, $2, NULL, 0);
    }
    | '-' a_expr %prec UMINUS
    {
        $$ = (Node *)makeAExpr(AEXPR_OP, 
              makeIntConst(0), $2, '-');
    }
    ;

%%
```

### 7.3 bison 优先级机制解释

```
表达式：1 + 2 * 3 > 5 AND name = 'Alice'

优先级声明：
  %left KW_OR              ← 最低
  %left KW_AND
  %right KW_NOT
  %nonassoc = < > <= >= <>  ← 比较运算
  %left + -
  %left * /                ← 最高
  %right UMINUS

bison 自动处理：
  1 + 2 * 3     → 1 + (2 * 3)      （* 优先级 > +）
  2 * 3 > 5     → (2 * 3) > 5      （> 优先级 < *）
  ... AND ...   → (... AND ...)     （AND 优先级最低）
```

`%left` 同时声明了结合性（左结合），例如 `a - b - c` 被解析为 `(a - b) - c`。

---

## 8. 构建系统

### 8.1 Makefile

```makefile
# Makefile - SLDB 构建文件
CC       = gcc
CFLAGS   = -Wall -Wextra -g -I./src/include
LDFLAGS  =

# flex & bison
LEX      = flex
YACC     = bison
LFLAGS   = # flex 选项
YFLAGS   = -d  # -d: 生成 gram.h (token 定义)

SRCDIR   = src
PARSERDIR = $(SRCDIR)/parser

# 自动生成的文件
GEN_SRC  = $(PARSERDIR)/scan.c $(PARSERDIR)/gram.c
GEN_HDR  = $(PARSERDIR)/gram.h

# 源文件
SRCS     = $(wildcard $(SRCDIR)/*.c) \
           $(wildcard $(SRCDIR)/nodes/*.c) \
           $(wildcard $(SRCDIR)/mem/*.c) \
           $(GEN_SRC)

OBJS     = $(SRCS:.c=.o)

TARGET   = sldb

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# flex: .l → .c
$(PARSERDIR)/scan.c: $(PARSERDIR)/scan.l
	$(LEX) $(LFLAGS) -o $@ $<

# bison: .y → .c + .h
$(PARSERDIR)/gram.c $(PARSERDIR)/gram.h: $(PARSERDIR)/gram.y
	$(YACC) $(YFLAGS) -o $(PARSERDIR)/gram.c $<

# 编译规则
%.o: %.c $(GEN_HDR)
	$(CC) $(CFLAGS) -c -o $@ $<

# gram.c 编译时关闭某些 flex 的警告
$(PARSERDIR)/gram.o: $(PARSERDIR)/gram.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

# 清理
clean:
	rm -f $(OBJS) $(GEN_SRC) $(GEN_HDR) $(TARGET)

# 测试
test: $(TARGET)
	@for f in test/sql/*.sql; do \
		echo "=== Testing $$f ==="; \
		./sldb < $$f; \
	done

.PHONY: all clean test
```

---

## 9. 实现路线图

| 阶段              | 任务                             | 产出                                        | 预计工作量 |
| ----------------- | -------------------------------- | ------------------------------------------- | ---------- |
| **Phase 1** | 搭建构建系统 + flex 基础词法分析 | Makefile, scan.l 能识别关键字和符号         | 1 天       |
| **Phase 2** | AST 节点系统 + 内存管理          | nodes.h, parsenodes.h, primnodes.h, arena.c | 1 天       |
| **Phase 3** | bison 骨架 + SELECT 完整语法     | gram.y SELECT 产生式, 表达式层次            | 1-2 天     |
| **Phase 4** | INSERT/UPDATE/DELETE 语法        | gram.y DML 产生式                           | 1 天       |
| **Phase 5** | CREATE/DROP TABLE 语法           | gram.y DDL 产生式                           | 0.5 天     |
| **Phase 6** | AST 输出/打印 + 测试用例         | 解析后打印 AST, SQL 测试文件                | 1 天       |
| **Phase 7** | 错误处理完善 + 边界测试          | 语法错误提示, 边界用例                      | 1 天       |

**总计**：约 1.5-2 周（业余时间）。

---

## 10. 关键技术细节

### 10.1 flex/reentrant 模式

```c
/*
 * flex 支持 %option reentrant（可重入），使词法分析器线程安全。
 * 不使用全局变量，状态通过 ScannerState 传入。
 *
 * 启用 reentrant 后：
 *   - yylex() 变为 yylex(YYSTYPE *yylval_param, yyscan_t yyscanner)
 *   - 通过 yylex_init() / yylex_destroy() 管理 scanner 实例
 *
 * bison 配合使用 %define api.pure 和 %parse-param
 */

/* gram.y */
%define api.pure full
%parse-param { void *yyscanner }

/* 在调用处：*/
yyscan_t scanner;
yylex_init(&scanner);
scanner_set_input(scanner, sql_text);
yyparse(scanner);
yylex_destroy(scanner);
```

### 10.2 PostgreSQL 的核心思想复现

| PostgreSQL 特性      | sldb 简化实现 | 说明                         |
| -------------------- | ------------- | ---------------------------- |
| `makeNode(type)`   | 同名宏        | Arena 分配 + 设 NodeTag      |
| `palloc()`         | Arena alloc   | 整块内存分配，解析后统一释放 |
| `MemoryContext`    | Arena         | 简化：单个 Arena 实例        |
| `List` (pg_list.h) | list.c        | 动态数组实现的链表           |
| `NodeTag` 枚举     | 同名          | 运行时类型识别               |
| `copyObject()`     | 暂不实现      | 后续需要时添加               |
| `equal()`          | 暂不实现      | 后续需要时添加               |

### 10.3 内存管理策略

```c
/*
 * Arena 分配器（简化版 MemoryContext）
 *
 * 设计原理：
 *   解析过程中频繁分配/释放小内存块。
 *   使用 Arena：一次分配一大块，所有 AST 节点从 Arena 分配。
 *   解析完成后统一释放整个 Arena。
 *
 * 优势：
 *   - 无碎片
 *   - 分配极快（指针递增）
 *   - 无需逐节点释放
 */

typedef struct Arena {
    char *memory;       /* 底层内存块 */
    size_t size;        /* 总大小 */
    size_t offset;      /* 已用偏移 */
} Arena;

Arena *arena_create(size_t size);
void  *arena_alloc(Arena *arena, size_t size);
void   arena_destroy(Arena *arena);

/* makeNode 宏：分配节点并设置类型 */
#define makeNode(_type_) \
    ({ \
        _type_ *_n = (_type_ *)arena_alloc(current_arena, sizeof(_type_)); \
        memset(_n, 0, sizeof(_type_)); \
        _n->type = T_##_type_; \
        _n; \
    })

/* 便捷函数 */
Node *makeString(const char *str);
Node *makeIntConst(long val);
Node *makeStringConst(const char *str);
Node *makeNullConst(void);
A_Expr *makeAExpr(A_Expr_Kind kind, Node *lexpr, Node *rexpr, int oper);
TypeName *makeTypeName(const char *name);
```

### 10.4 错误处理

```c
/*
 * 参考 PostgreSQL elog/ereport 错误报告机制
 *
 * 简化版：解析错误时记录错误信息，设置错误码
 */

/* 在 gram.y 中 */
%error-verbose   /* bison 自动生成详细错误信息 */

#define parser_err(msg, ...) \
    do { \
        snprintf(pstate->err_msg, sizeof(pstate->err_msg), msg, ##__VA_ARGS__); \
        YYERROR; \
    } while(0)

/* 使用示例（在语法动作中）*/
| KW_CREATE KW_TABLE IDENT '(' column_def_list ')'
    {
        if (list_length($5) == 0)
            parser_err("CREATE TABLE must have at least one column");
        /* ... */
    }

/* yyerror 辅助函数 */
void yyerror(YYLTYPE *loc, void *scanner, ParserState *pstate, const char *msg)
{
    snprintf(pstate->err_msg, 512, "line %d: %s", loc->first_line, msg);
}
```

---

## 11. 与 PostgreSQL 的差异

| 维度                    | PostgreSQL                    | sldb（本方案）                      |
| ----------------------- | ----------------------------- | ----------------------------------- |
| **关键字处理**    | ScanKeywordLookup + 二分查找  | scan_keyword + 线性查找（后续优化） |
| **语法规则数**    | ~5000 行 gram.y               | ~400 行 gram.y                      |
| **表达式层次**    | 5 层 (a/b/c/d/e_expr)         | 3 层 (a/b/c_expr)                   |
| **数据类型**      | 丰富（~50 种）                | INT, TEXT, VARCHAR                  |
| **子查询**        | 支持                          | 暂不支持                            |
| **JOIN**          | 支持所有类型                  | 暂不支持                            |
| **A_Expr 运算符** | 按 name 列表查找              | 直接存储 token 值                   |
| **内存管理**      | MemoryContext + palloc        | 简化 Arena                          |
| **Node 工具**     | copy/equal/outfuncs/readfuncs | 仅基础实现                          |

这些简化是有意为之：sldb 的目标是**理解核心原理**，而非实现完整 SQL 标准。当需要扩展功能时（如增加子查询），PostgreSQL 的相同架构确保扩展路径平滑。

---

## 12. 总结

| 决策                   | 选择               | 核心理由                                        |
| ---------------------- | ------------------ | ----------------------------------------------- |
| **编程语言**     | C                  | 与 PostgreSQL/SQLite 一致，可直接参考工业级源码 |
| **词法分析**     | flex               | 高效 DFA 自动生成，避免手写状态机               |
| **语法分析**     | bison (LALR(1))    | BNF 即文档，自动处理左递归和优先级              |
| **AST 节点系统** | NodeTag + struct   | 运行时类型安全，参考 PostgreSQL 成熟设计        |
| **内存管理**     | Arena 分配器       | 解析期内高效分配，统一释放                      |
| **表达式优先级** | bison %left/%right | 一行声明，自动解决 shift/reduce 冲突            |
| **集合数据结构** | List (动态数组)    | 参考 pg_list.h，简化实现                        |

**核心设计哲学**：

> 在每一个设计决策上，都向 PostgreSQL 靠拢。这让 sldb 不仅是"一个能跑的数据库"，更是"理解 PostgreSQL 源码的阶梯"。
>
> 当你掌握了 sldb 的 flex/bison 解析器后，打开 PostgreSQL `src/backend/parser/gram.y` 将不再是一片陌生——你会看到相同的 NodeTag 系统、相同的 List 操作、相同的表达式层次设计，只是规模更大、细节更多。

**学习路径**：

1. sldb 解析器 (本文档) → 理解 flex/bison + AST 节点系统
2. PostgreSQL `scan.l` / `gram.y` → 理解工业级扩展（子查询/JOIN/窗口函数）
3. PostgreSQL `src/backend/parser/analyze.c` → 理解语义分析（名称解析/类型推导）
4. PostgreSQL `src/backend/optimizer/` → 理解查询优化（重写/代价估算/计划生成）
