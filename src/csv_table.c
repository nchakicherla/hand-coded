#include <stdbool.h>
#include <stdalign.h>
#include <string.h>

#include "csv.h"

#include "arena.h"
#include "file.h"
#include "csv_table.h"

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

	CsvCounter *counter = (CsvCounter *)data;

	counter->col++;
	counter->total_bytes += field_len;
	return;
}

static void cb_row_counter(int c, void *data) {
	(void)c;
	CsvCounter *counter = (CsvCounter *)data;

	if (counter->row == 0) { // expected # cols based off first row
		counter->num_cols = counter->col;
	}

	if (counter->col != counter->num_cols) {
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

CsvTableError csv_table_load(const char *path, Table *out_table) {
	Arena scratch;
	arena_init(&scratch);

	size_t csv_size;
	char *csv_buffer = file_read_all(&scratch, path, &csv_size);
	if (!csv_buffer) {
		arena_term(&scratch);
		return CSV_TABLE_ERR_FILE;
	}

	CsvCounter counter = {0};

	struct csv_parser parser;
	csv_init(&parser, 0);
	csv_parse(&parser, csv_buffer, csv_size, cb_field_counter, cb_row_counter, &counter);
	csv_fini(&parser, cb_field_counter, cb_row_counter, &counter);

	if (counter.jagged_csv) {
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_JAGGED;
	}

	CsvCopier copier = {0};

	copier.data = arena_alloc(&scratch, counter.total_bytes + 1, alignof(char));
	copier.offsets = arena_alloc(&scratch, (counter.num_cols * counter.num_rows + 1) * sizeof(size_t), alignof(size_t));

	csv_parse(&parser, csv_buffer, csv_size, cb_field_copier, NULL, &copier);
	csv_fini(&parser, cb_field_copier, NULL, &copier);
	csv_free(&parser);

	copier.data[counter.total_bytes] = '\0';
	copier.offsets[copier.cell_index] = copier.byte_cursor;

	size_t *col_lens = arena_zalloc(&scratch, counter.num_cols * sizeof(size_t), alignof(size_t));

	for (size_t i = 0; i < counter.num_cols * counter.num_rows; i++) {
		size_t field_len = copier.offsets[i + 1] - copier.offsets[i];
		col_lens[i % counter.num_cols] += field_len + 1; // account for null-termination
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

	size_t *col_cursors = arena_zalloc(&scratch, counter.num_cols * sizeof(size_t), alignof(size_t));

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

	arena_term(&scratch);

	*out_table = table;
	return CSV_TABLE_OK;
}

void csv_table_free(Table *table) {
	arena_term(&table->arena);
	return;
}
