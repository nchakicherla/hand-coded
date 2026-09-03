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

typedef struct {
	char *data;
	char **rows;
} Column;

typedef struct {
	Column *columns;
	Arena arena;
	size_t num_cols;
	size_t num_rows;
} Table;

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
		csv_free(&parser);
		arena_term(&arena);
		return 1;
	}

	CsvCopier copier = {0};

	copier.data = arena_alloc(&arena, counter.total_bytes + 1, alignof(char));
	copier.offsets = arena_alloc(&arena, (counter.num_cols * counter.num_rows + 1) * sizeof(size_t), alignof(size_t));

	csv_parse(&parser, csv_buffer, csv_size, cb_field_copier, NULL, &copier);
	csv_fini(&parser, cb_field_copier, NULL, &copier);
	csv_free(&parser);

	copier.data[counter.total_bytes] = '\0';
	copier.offsets[copier.cell_index] = copier.byte_cursor;

	for (size_t i = 0; i < counter.num_rows * counter.num_cols; i++) {
		if (i % counter.num_cols == 0) {
			printf("-- row %zu --\n", i / counter.num_cols);
		}
		printf("-- col %zu --\n", i % counter.num_cols);
		printf("%.*s ", (int)(copier.offsets[i + 1] - copier.offsets[i]), &copier.data[copier.offsets[i]]);
		printf("[cell len: %zu]\n", copier.offsets[i + 1] - copier.offsets[i]);
	}

	size_t *col_lens = arena_alloc(&arena, counter.num_cols * sizeof(size_t), alignof(size_t));

	for (size_t i = 0; i < counter.num_cols * counter.num_rows; i++) {
		size_t field_len = copier.offsets[i + 1] - copier.offsets[i];
		col_lens[i % counter.num_cols] += field_len + 1; // account for null-termination
	}

	for (size_t i = 0; i < counter.num_cols; i++) {
		printf("col %zu len: %zu\n", i, col_lens[i]);
	}

	Table table = {0};
	arena_init(&table.arena);

	table.num_cols = counter.num_cols;
	table.num_rows = counter.num_rows;
	table.columns = arena_alloc(&table.arena, table.num_cols * sizeof(Column), alignof(Column));

	for (size_t i = 0; i < table.num_cols; i++) {
		table.columns[i].data = arena_alloc(&table.arena, col_lens[i], alignof(char));
		table.columns[i].rows = arena_alloc(&table.arena, table.num_rows * sizeof(char *), alignof(char *));
	}

	size_t *col_cursors = arena_zalloc(&arena, counter.num_cols * sizeof(size_t), alignof(size_t));

	for (size_t i = 0; i < counter.num_rows * counter.num_cols; i++) {
		size_t row = i / counter.num_cols;
		size_t col = i % counter.num_cols;
		size_t field_len = copier.offsets[i + 1] - copier.offsets[i];

		char *dest = table.columns[col].data + col_cursors[col];
		memcpy(dest, &copier.data[copier.offsets[i]], field_len);
		dest[field_len] = '\0';

		table.columns[col].rows[row] = dest;
		col_cursors[col] += field_len + 1;
	}

	printf("print row 2...\n");

	for (size_t i = 0; i < table.num_cols; i++) {
		printf("%s ", table.columns[i].rows[2]);
	}

	arena_term(&arena);
	return 0;
}
