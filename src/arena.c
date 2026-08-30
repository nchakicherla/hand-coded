#include "arena.h"

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdalign.h>

#define MEMORY_HOG_FACTOR 4
#define DEF_BLOCK_SIZE 4096

static ArenaBlock *alloc_init_block(size_t block_size) {

	ArenaBlock *block =  malloc(sizeof(ArenaBlock));
	if(!block) {
		fprintf(stderr, "block alloc failed! exiting...\n");
		exit(1);
	}

	block->data = malloc(block_size);
	if(!block->data) {
		fprintf(stderr, "block data alloc failed! exiting...\n");
		exit(2);
	}

	block->data_size = block_size;
	block->next = NULL;

	return block;
}

int arena_init(Arena *a) {
	size_t block_size = DEF_BLOCK_SIZE;

	a->first_block = alloc_init_block(block_size);

	a->bytes_used = 0;
	a->bytes_allocd = sizeof(Arena) + sizeof(ArenaBlock) + block_size;
	a->next_free = a->first_block->data;
	a->next_free_size = a->first_block->data_size;
	a->last_block_size = block_size;
	a->last_block = a->first_block;
	return 0;
}

int arena_term(Arena *a) {

	ArenaBlock *curr = a->first_block;
	ArenaBlock *next = NULL;

	while(curr) {
		next = curr->next;
		free(curr->data);
		free(curr);
		curr = next;
	}
	return 0;
}

int arena_reset(Arena *a) { // preserves last_block_size from pre-reset

	ArenaBlock *curr = a->first_block;
	ArenaBlock *next = NULL;

	while(curr) {
		next = curr->next;
		free(curr->data);
		free(curr);
		curr = next;
	}

	a->first_block = alloc_init_block(a->last_block_size);
	a->last_block = a->first_block;

	a->next_free = a->first_block->data;
	a->next_free_size = a->first_block->data_size;

	a->bytes_used = 0;
	a->bytes_allocd = sizeof(Arena) + sizeof(ArenaBlock) + a->last_block_size;
	return 0;
}

void *arena_alloc(Arena *a, size_t size, size_t alignment) {

	// bump up per alignment
	size_t current = (size_t)a->next_free;
	size_t aligned = (current + alignment - 1) & ~(alignment - 1);
	size_t padding = aligned - current;

	if(a->next_free_size < size + padding) {
		ArenaBlock *last_block = a->last_block;
		ArenaBlock *new_block = NULL;

		size_t new_block_size = a->last_block_size;

		while(new_block_size < size * MEMORY_HOG_FACTOR) {
			new_block_size = new_block_size * 2;
		}

		new_block = alloc_init_block(new_block_size);
		if(!new_block) {
			exit(3); // alloc_init_block itself should exit() if malloc fails but putting exit(3) here anyway
		}
		last_block->next = new_block;
		last_block = new_block;

		a->last_block = new_block;
		a->last_block_size = new_block_size;
		a->next_free = (char *)last_block->data + size;
		a->next_free_size = new_block_size - size;

		a->bytes_used += size + padding;
		a->bytes_allocd += sizeof(ArenaBlock) + new_block_size;

		return last_block->data;
	}

	void *output = (void *)aligned;
	a->next_free = (void *)(aligned + size);
	a->next_free_size  = a->next_free_size - (size + padding);

	a->bytes_used += size + padding;

	return output;
}

void *arena_zalloc(Arena *a, size_t size, size_t alignment) {
	void* output = arena_alloc(a, size, alignment);
	memset(output, 0, size);
	return output;
}

void *arena_grow_alloc(void *ptr, size_t old_size, size_t new_size, Arena *a) {
	void *output_ptr = arena_alloc(a, new_size, 32); // use default alignment of 32
	memcpy(output_ptr, ptr, old_size);
	return output_ptr;
}

char *arena_new_str(char *str, Arena *a) {

	char *output = NULL;
	size_t len = strlen(str);

	output = arena_alloc(a, len + 1, alignof(char));

	memcpy(output, str, len);
	output[len] = '\0';
	return output;
}

size_t arena_get_bytes_used(Arena *a) {
	return a->bytes_used;
}

size_t arena_get_bytes_allocd(Arena *a) {
	return a->bytes_allocd;
}

void arena_print_info(Arena *a) {
	printf("\nMEMPOOL INFO - \n");
	printf("\tUSED: %zu, (%f MB)\n", arena_get_bytes_used(a), (double)arena_get_bytes_used(a) / (1024 * 1024));
	printf("\tALLOCD: %zu, (%f MB)\n", arena_get_bytes_allocd(a), (double)arena_get_bytes_allocd(a) / (1024 * 1024));
	return;
}
