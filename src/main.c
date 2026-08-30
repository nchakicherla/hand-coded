#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>

#include "arena.h"

int main(void) {
	printf("hello, world\n");

	Arena arena;
	arena_init(&arena);

	char *test_alloc = arena_alloc(&arena, 1000000, alignof(char));

	for (size_t i = 0; i < 1000000; i++) {
		test_alloc[i] = 'a';
		printf("%c", test_alloc[i]);
	}
	printf("\n");

	arena_term(&arena);
	return 0;
}