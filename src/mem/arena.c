#include "mem.h"
#include <stdlib.h>
#include <string.h>

/* 全局当前 Arena 指针，makeNode 等宏通过它确定在哪个内存池上分配节点 */
Arena *current_arena = NULL;

/* 创建（初始化）一个 Arena 内存池。
 * 初始时没有任何内存块（chunks 为空），分配时才按需申请。 */
Arena *arena_create(void) {
    Arena *a = (Arena *)malloc(sizeof(Arena));
    
    if (!a) return NULL;
    a->chunks = NULL;   /* 尚未分配任何内存块 */
    
    return a;
}

/* 内部函数：新建一个大小为 size 的内存块（chunk）。
 * 一块 chunk 由连续 base 缓冲区、总大小 size、已用字节 used 及指向下一块的 next 组成。 */
static ArenaChunk *new_chunk(size_t size) {
    ArenaChunk *c = (ArenaChunk *)malloc(sizeof(ArenaChunk));
    
    if (!c) return NULL;
    
    c->base = (char *)malloc(size);   /* 实际的连续内存缓冲区 */
    c->size = size;                   /* 缓冲区总容量（字节） */
    c->used = 0;                      /* 已分配出去的字节数 */
    c->next = NULL;                   /* 链表指针，指向下一块 chunk */
    
    return c;
}

/* 从 Arena 中分配 size 字节，返回对齐后的内存地址。
 * 采用“线性 bump 分配”策略：在当前 chunk 末尾直接推进 used 指针即可。 */
void *arena_alloc(Arena *arena, size_t size) {
    /* 将请求大小向上对齐到 8 字节边界，保证返回地址满足基本对齐要求 */
    size = (size + 7) & ~((size_t)7);
    if (size == 0) size = 8;   /* 避免请求 0 字节时返回非法空指针 */

    ArenaChunk *c = arena->chunks;
    if (!c) {
        /* 还没有任何 chunk，申请第一块（最小 4096 字节，或满足本次请求） */
        c = new_chunk(size > 4096 ? size : 4096);
        arena->chunks = c;
    } else {
        /* 已有 chunk，找到链表末尾的 chunk（新分配总是追加在末尾） */
        while (c->next) c = c->next;
    }

    /* 当前 chunk 剩余空间不足，则新开一块更大的 chunk。
     * 新块大小取“当前块翻倍”与“请求大小”中的较大者，以降低碎片、避免频繁扩容。 */
    if (c->used + size > c->size) {
        size_t ns = c->size * 2;
        if (ns < size) ns = size;

        ArenaChunk *nc = new_chunk(ns);
        c->next = nc;
        c = nc;
    }

    /* bump 分配：在 used 处切出 size 字节，并把 used 向后推进 */
    void *p = c->base + c->used;
    c->used += size;

    return p;
}

/* 分配并清零（等价于 calloc）：分配后把整块内存置 0，便于节点字段得到默认零值。 */
void *arena_alloc0(Arena *arena, size_t size) {
    void *p = arena_alloc(arena, size);
    memset(p, 0, size);

    return p;
}

/* 在 Arena 内复制字符串：分配 len+1 字节并把源字符串（含结尾 '\0'）拷入，
 * 这样字符串的生命周期与 Arena 一致，无需单独 free。 */
char *arena_strdup(Arena *arena, const char *s) {
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *d = (char *)arena_alloc(arena, len);
    memcpy(d, s, len);
    
    return d;
}

/* 销毁整个 Arena：释放所有 chunk 的缓冲区与描述结构，最后释放 Arena 本身。
 * 因为采用 Arena 整体管理，所以无需逐个释放节点，一次调用即可回收全部内存。 */
void arena_destroy(Arena *arena) {
    ArenaChunk *c = arena->chunks;
    while (c) {
        ArenaChunk *n = c->next;
        free(c->base);   /* 释放缓冲区 */
        free(c);         /* 释放 chunk 描述结构 */
        c = n;
    }
    free(arena);
}

/* 重置 Arena：不清空内存，仅把每块 chunk 的 used 归零。
 * 之后的分配会从头复用已有缓冲区，适合“反复解析、批量释放”的场景，避免重复申请。 */
void arena_reset(Arena *arena) {
    for (ArenaChunk *c = arena->chunks; c; c = c->next)
        c->used = 0;
}

/* 设置 / 获取全局当前 Arena，供 makeNode 等宏在分配节点时使用。 */
void set_current_arena(Arena *arena) { current_arena = arena; }
Arena *get_current_arena(void) { return current_arena; }
