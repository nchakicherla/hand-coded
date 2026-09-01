#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include <string.h>

struct s_arena_block;

typedef struct s_arena {
	size_t bytes_used;
	size_t bytes_allocd;

	void *next_free;
	size_t next_free_size;
	size_t last_block_size;
	struct s_arena_block *last_block;
	struct s_arena_block *first_block;
} Arena;

typedef struct s_arena_block {
	void *data;
	size_t data_size;
	struct s_arena_block *next;
} ArenaBlock;

int arena_init(Arena *a);
int arena_term(Arena *a);
int arena_reset(Arena *a);
void *arena_alloc(Arena *a, size_t size, size_t alignment);
void *arena_zalloc(Arena *a, size_t size, size_t alignment);
void *arena_grow_alloc(void *ptr, size_t old_size, size_t new_size, Arena *a);
void *arena_grow_alloc_zeroed(void *ptr, size_t old_size, size_t new_size, Arena *a);
char *arena_new_str(char *str, Arena *a);
size_t arena_get_bytes_used(Arena *a);
size_t arena_get_bytes_allocd(Arena *a);
void arena_print_info(Arena *a);

#endif // ARENA_H
