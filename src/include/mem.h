#ifndef MEM_H
#define MEM_H

#include <stddef.h>

/*
 * 简化版 Arena（参考 PostgreSQL MemoryContext）
 *
 * 解析过程中频繁分配小内存块。使用分块 Arena：
 *   - 一次分配一块较大的内存，节点从其中分配
 *   - 多个 Chunk 链式增长，旧指针始终保持有效（不会 realloc 搬迁）
 *   - 解析完成后统一释放整个 Arena
 */
typedef struct ArenaChunk {
    char *base;
    size_t size;
    size_t used;
    struct ArenaChunk *next;
} ArenaChunk;

typedef struct Arena {
    ArenaChunk *chunks;
} Arena;

/* 当前解析使用的 Arena（makeNode 依赖此全局） */
extern Arena *current_arena;

Arena *arena_create(void);
void  *arena_alloc(Arena *arena, size_t size);
void  *arena_alloc0(Arena *arena, size_t size);
char  *arena_strdup(Arena *arena, const char *s);
void   arena_destroy(Arena *arena);
void   arena_reset(Arena *arena);
void   set_current_arena(Arena *arena);
Arena *get_current_arena(void);

#endif
