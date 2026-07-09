#ifndef PG_LIST_H
#define PG_LIST_H

#include "nodes.h"

/*
 * List - 链表（参考 PostgreSQL pg_list.h）
 *
 * 内部用动态数组存储 void*，支持前插/后插/索引访问/遍历。
 */
typedef struct ListCell {
    void *data;
} ListCell;

typedef struct List {
    NodeTag  type;     /* = T_List */
    int      length;
    int      capacity;
    ListCell *elements;
} List;

List *make_list(void);
List *make_list_with_capacity(int capacity);
List *lappend(List *list, void *datum);
List *lcons(void *datum, List *list);
void *list_nth(const List *list, int n);
void *linitial(const List *list);
void *llast(const List *list);

#define list_length(l)  (((l) != NULL) ? (l)->length : 0)

/* 便捷构造宏 */
#define makeList1(x1)  lappend(make_list(), (x1))
#define makeList2(x1, x2)  lappend(lappend(make_list(), (x1)), (x2))

#endif
