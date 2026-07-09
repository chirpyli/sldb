#ifndef NODE_H
#define NODE_H

#include "nodes.h"
#include "primnodes.h"

/* AST 节点构造辅助函数 */
String   *makeString(char *str);
A_Const  *makeIntConst(long val);
A_Const  *makeStringConst(char *str);
A_Const  *makeNullConst(void);
A_Expr   *makeAExpr(A_Expr_Kind kind, Node *lexpr, Node *rexpr, int oper);
TypeName *makeTypeName(char *name);

#endif
