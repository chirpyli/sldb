#ifndef NODES_H
#define NODES_H

#include "mem.h"

/*
 * NodeTag - 节点类型标识（参考 PostgreSQL nodes.h）
 *
 * 每个 AST 节点的第一个字段必须是 NodeTag type。
 * 通过将任意节点指针转为 (Node *) 即可读取其类型。
 */
typedef enum NodeTag {
    T_Invalid = 0,
    T_RawStmt,
    T_SelectStmt,
    T_InsertStmt,
    T_UpdateStmt,
    T_DeleteStmt,
    T_CreateStmt,
    T_DropStmt,
    T_ColumnRef,
    T_A_Expr,
    T_A_Const,
    T_TypeCast,
    T_RangeVar,
    T_ResTarget,
    T_ColumnDef,
    T_TypeName,
    T_SortBy,
    T_SetClause,
    T_String,
    T_List,
} NodeTag;

/* 所有 AST 节点的基类型 */
typedef struct Node {
    NodeTag type;
} Node;

#define IsA(nodeptr, tag)  (((const Node *)(nodeptr))->type == (tag))
#define castNode(type, nodeptr)  ((type *)(nodeptr))

/* makeNode 实现（见 node.c） */
void *makeNode_impl(NodeTag tag, size_t size);

/* 分配一个类型为 type 的节点并清零，设置 NodeTag */
#define makeNode(type)  ((type *)makeNode_impl(T_##type, sizeof(type)))

#endif
