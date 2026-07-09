#include "pg_list.h"
#include "mem.h"

List *make_list_with_capacity(int capacity) {
    List *list = (List *)arena_alloc0(current_arena, sizeof(List));
    list->type = T_List;
    list->length = 0;
    list->capacity = capacity > 0 ? capacity : 4;
    list->elements = (ListCell *)arena_alloc0(current_arena,
                                              sizeof(ListCell) * list->capacity);
    return list;
}

List *make_list(void) {
    return make_list_with_capacity(4);
}

static List *list_enlarge(List *list) {
    if (list->length < list->capacity) return list;
    int newcap = list->capacity * 2;
    ListCell *newelems = (ListCell *)arena_alloc0(current_arena,
                                                  sizeof(ListCell) * newcap);
    for (int i = 0; i < list->length; i++)
        newelems[i] = list->elements[i];
    list->elements = newelems;
    list->capacity = newcap;
    return list;
}

List *lappend(List *list, void *datum) {
    if (list == NULL) list = make_list();
    list = list_enlarge(list);
    list->elements[list->length].data = datum;
    list->length++;
    return list;
}

List *lcons(void *datum, List *list) {
    if (list == NULL) list = make_list();
    list = list_enlarge(list);
    for (int i = list->length; i > 0; i--)
        list->elements[i] = list->elements[i - 1];
    list->elements[0].data = datum;
    list->length++;
    return list;
}

void *list_nth(const List *list, int n) {
    if (list == NULL || n < 0 || n >= list->length) return NULL;
    return list->elements[n].data;
}

void *linitial(const List *list) {
    return list_nth(list, 0);
}

void *llast(const List *list) {
    return list_nth(list, list->length - 1);
}
