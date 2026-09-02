#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "csv.h"

#include "arena.h"
#include "file.h"

typedef struct {
	size_t col;
	size_t row;
	size_t num_cols;
	size_t num_rows;

	bool jagged_csv;
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

	if (counter->row == 0) { // expected # cols based off first row
		counter->num_cols = counter->col;
	}

	if (counter->col != counter->num_cols) {
		fprintf(stderr, "error: jagged CSV\n");
		counter->jagged_csv = true;
	}

	counter->row++;
	counter->col = 0;

	counter->num_rows = counter->row;
	return;
}

int main(void) {
	Arena arena;
	arena_init(&arena);

	size_t csv_size;
	const char *csv_path = "./resources/sample.csv";
	char *csv_buffer = file_read_all(&arena, csv_path, &csv_size);

	CsvCounter counter = { 
		.col = 0,
		.row = 0
	};

	struct csv_parser parser;
	csv_init(&parser, 0);
	csv_parse(&parser, csv_buffer, csv_size, cb_field, cb_row, &counter);
	csv_fini(&parser, cb_field, cb_row, &counter);
	csv_free(&parser);

	printf("total num rows: %zu\n", counter.num_rows);
	printf("total num cols: %zu\n", counter.num_cols);

	if (counter.jagged_csv) {
		fprintf(stderr, "error: aborting due to jagged CSV: %s\n", csv_path);
		arena_term(&arena);
		return 1;
	}

	arena_term(&arena);
	return 0;
}
