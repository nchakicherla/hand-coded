#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdalign.h>

#include "csv.h"

#include "arena.h"
#include "file.h"

typedef struct {
	size_t col;
	size_t row;
	size_t num_cols;
	size_t num_rows;
	size_t total_bytes;

	bool jagged_csv;
} CsvCounter;

static void cb_field_counter(void *field, size_t field_len, void *data) {
	(void)field;
	(void)field_len;

	CsvCounter *counter = (CsvCounter *)data;

	printf("%zu ", counter->col);
	counter->col++;
	counter->total_bytes += field_len;
	return;
}

static void cb_row_counter(int c, void *data) {
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

typedef struct {
	char *data;
	size_t *offsets;
	size_t byte_cursor;
	size_t cell_index;
} CsvCopier;

static void cb_field_copier(void *field, size_t field_len, void *data) {
	CsvCopier *copier = (CsvCopier *)data;
	
	copier->offsets[copier->cell_index] = copier->byte_cursor;
	memcpy(copier->data + copier->byte_cursor, field, field_len);
	
	copier->byte_cursor += field_len;
	copier->cell_index++;
	return;
}

int main(void) {
	Arena arena;
	arena_init(&arena);

	size_t csv_size;
	const char *csv_path = "./resources/sample.csv";
	char *csv_buffer = file_read_all(&arena, csv_path, &csv_size);

	CsvCounter counter = {0};

	struct csv_parser parser;
	csv_init(&parser, 0);
	csv_parse(&parser, csv_buffer, csv_size, cb_field_counter, cb_row_counter, &counter);
	csv_fini(&parser, cb_field_counter, cb_row_counter, &counter);

	printf("total num rows: %zu\n", counter.num_rows);
	printf("total num cols: %zu\n", counter.num_cols);
	printf("total bytes: %zu\n", counter.total_bytes);

	if (counter.jagged_csv) {
		fprintf(stderr, "error: aborting due to jagged CSV: %s\n", csv_path);
		arena_term(&arena);
		return 1;
	}

	CsvCopier copier = {0};

	copier.data = arena_alloc(&arena, counter.total_bytes + 1, alignof(char));
	copier.offsets = arena_alloc(
		&arena,
		(counter.num_cols * counter.num_rows + 1) * sizeof(size_t), 
		alignof(size_t)
	);

	csv_parse(&parser, csv_buffer, csv_size, cb_field_copier, NULL, &copier);
	csv_fini(&parser, cb_field_copier, NULL, &copier);
	csv_free(&parser);

	copier.data[counter.total_bytes] = '\0';
	copier.offsets[copier.cell_index] = copier.byte_cursor;

	arena_term(&arena);
	return 0;
}
