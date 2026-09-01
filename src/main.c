#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "file.h"

int main(void) {
	printf("hello, world\n");

	Arena arena;
	arena_init(&arena);

	arena_term(&arena);
	return 0;
}