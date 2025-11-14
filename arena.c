/*
 * Arena Allocator Implementation
 */

#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global arena for autograd */
Arena *global_arena = NULL;

/* Create new arena */
Arena* arena_create(size_t initial_chunk_size) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) return NULL;

    /* Default chunk size: 10MB */
    if (initial_chunk_size == 0) {
        initial_chunk_size = 10 * 1024 * 1024;
    }

    /* Allocate first chunk */
    ArenaChunk *chunk = malloc(sizeof(ArenaChunk));
    if (!chunk) {
        free(arena);
        return NULL;
    }

    chunk->memory = malloc(initial_chunk_size);
    if (!chunk->memory) {
        free(chunk);
        free(arena);
        return NULL;
    }

    chunk->size = initial_chunk_size;
    chunk->used = 0;
    chunk->next = NULL;

    arena->first = chunk;
    arena->current = chunk;
    arena->chunk_size = initial_chunk_size;
    arena->total_allocated = initial_chunk_size;
    arena->total_used = 0;

    return arena;
}

/* Destroy arena and free all memory */
void arena_destroy(Arena *arena) {
    if (!arena) return;

    ArenaChunk *chunk = arena->first;
    while (chunk) {
        ArenaChunk *next = chunk->next;
        free(chunk->memory);
        free(chunk);
        chunk = next;
    }

    free(arena);
}

/* Allocate memory from arena */
void* arena_alloc(Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    /* Align to 8 bytes for better performance */
    size = (size + 7) & ~7;

    /* Check if current chunk has space */
    ArenaChunk *chunk = arena->current;
    if (chunk->used + size > chunk->size) {
        /* Need new chunk */
        size_t new_chunk_size = arena->chunk_size;
        if (size > new_chunk_size) {
            new_chunk_size = size * 2;  /* Ensure it fits */
        }

        ArenaChunk *new_chunk = malloc(sizeof(ArenaChunk));
        if (!new_chunk) return NULL;

        new_chunk->memory = malloc(new_chunk_size);
        if (!new_chunk->memory) {
            free(new_chunk);
            return NULL;
        }

        new_chunk->size = new_chunk_size;
        new_chunk->used = 0;
        new_chunk->next = NULL;

        /* Add to chain */
        chunk->next = new_chunk;
        arena->current = new_chunk;
        arena->total_allocated += new_chunk_size;

        chunk = new_chunk;
    }

    /* Allocate from current chunk */
    void *ptr = chunk->memory + chunk->used;
    chunk->used += size;
    arena->total_used += size;

    return ptr;
}

/* Allocate zeroed memory */
void* arena_calloc(Arena *arena, size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = arena_alloc(arena, total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* Reset arena - keep chunks but mark as unused */
void arena_reset(Arena *arena) {
    if (!arena) return;

    ArenaChunk *chunk = arena->first;
    while (chunk) {
        chunk->used = 0;
        chunk = chunk->next;
    }

    arena->current = arena->first;
    arena->total_used = 0;
}

/* Reset arena aggressively - free all chunks except the first */
void arena_reset_aggressive(Arena *arena) {
    if (!arena) return;

    /* Keep first chunk */
    ArenaChunk *first = arena->first;
    first->used = 0;

    /* Free all other chunks */
    ArenaChunk *chunk = first->next;
    while (chunk) {
        ArenaChunk *next = chunk->next;
        arena->total_allocated -= chunk->size;
        free(chunk->memory);
        free(chunk);
        chunk = next;
    }

    /* Reset pointers */
    first->next = NULL;
    arena->current = first;
    arena->total_used = 0;
}

/* Get memory usage stats */
size_t arena_get_used(Arena *arena) {
    if (!arena) return 0;
    return arena->total_used;
}

size_t arena_get_allocated(Arena *arena) {
    if (!arena) return 0;
    return arena->total_allocated;
}

/* Initialize global arena */
void arena_init_global(void) {
    if (!global_arena) {
        global_arena = arena_create(0);  /* Use default size */
        if (!global_arena) {
            fprintf(stderr, "ERROR: Failed to create global arena\n");
            exit(1);
        }
    }
}

/* Cleanup global arena */
void arena_cleanup_global(void) {
    if (global_arena) {
        arena_destroy(global_arena);
        global_arena = NULL;
    }
}