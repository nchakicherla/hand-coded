#include <stdio.h>
#include <stdlib.h>

#include "csv.h"

#include "arena.h"
#include "file.h"

typedef struct {
	size_t col;
	size_t max_col;
	size_t row;
} CsvCounter;

static void cb_field(void *field, size_t field_len, void *data) {
	(void)field;
	(void)field_len;

	CsvCounter *counter = (CsvCounter *)data;

	printf("%zu ", counter->col);
	counter->col++;
	return;
}

static void cb_row(int c, void *data) {
	(void)c;
	CsvCounter *counter = (CsvCounter *)data;
	printf("<-- row %zu\n", counter->row);
	counter->row++;
	if (counter->col > counter->max_col) {
		counter->max_col = counter->col;
	}
	counter->col = 0;
	return;
}

int main(void) {
	Arena arena;
	arena_init(&arena);

	size_t csv_size;
	char *csv_buffer = file_read_all(&arena, "./resources/sample.csv", &csv_size);

	CsvCounter counter = { 
		.col = 0,
		.max_col = 0,
		.row = 0
	};

	struct csv_parser parser;
	csv_init(&parser, 0);
	csv_parse(&parser, csv_buffer, csv_size, cb_field, cb_row, &counter);
	csv_fini(&parser, cb_field, cb_row, &counter);
	csv_free(&parser);

	printf("total num rows: %zu\n", counter.row);
	printf("max cols: %zu\n", counter.max_col);

	arena_term(&arena);
	return 0;
}
