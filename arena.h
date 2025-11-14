/*
 * Arena Allocator for Autograd System
 *
 * Provides fast bulk allocation/deallocation for temporary Variables/Tensors
 * All intermediate values are allocated from the arena and freed together
 */

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdbool.h>

typedef struct ArenaChunk ArenaChunk;
typedef struct Arena Arena;

/* Single memory chunk in the arena */
struct ArenaChunk {
    char *memory;           /* Allocated memory block */
    size_t size;           /* Size of this chunk */
    size_t used;           /* Bytes used so far */
    ArenaChunk *next;      /* Next chunk in linked list */
};

/* Arena allocator */
struct Arena {
    ArenaChunk *first;     /* First chunk */
    ArenaChunk *current;   /* Current chunk being allocated from */
    size_t chunk_size;     /* Default size for new chunks */
    size_t total_allocated; /* Total memory allocated */
    size_t total_used;     /* Total memory used */
};

/* Create/destroy arena */
Arena* arena_create(size_t initial_chunk_size);
void arena_destroy(Arena *arena);

/* Allocate memory from arena */
void* arena_alloc(Arena *arena, size_t size);
void* arena_calloc(Arena *arena, size_t count, size_t size);

/* Reset arena (free all allocations but keep chunks) */
void arena_reset(Arena *arena);

/* Reset arena aggressively (free all chunks except first) */
void arena_reset_aggressive(Arena *arena);

/* Get statistics */
size_t arena_get_used(Arena *arena);
size_t arena_get_allocated(Arena *arena);

/* Global arena for autograd temporary allocations */
extern Arena *global_arena;
void arena_init_global(void);
void arena_cleanup_global(void);

#endif /* ARENA_H */