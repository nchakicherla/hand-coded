#include <stdbool.h>
#include <stdalign.h>
#include <stdint.h>
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
	bool overflow;
} CsvCounter;

static bool checked_multiply(size_t a, size_t b, size_t *result) {
	if (a != 0 && b > SIZE_MAX / a) {
		return false;
	}
	*result = a * b;
	return true;
}

static bool checked_addition(size_t a, size_t b, size_t *result) {
	if (b > SIZE_MAX - a) {
		return false;
	}
	*result = a + b;
	return true;
}

static void cb_field_counter(void *field, size_t field_len, void *data) {
	(void)field;

	CsvCounter *counter = (CsvCounter *)data;

	if (counter->overflow) {
		return;
	}
	if (!checked_addition(counter->col, 1, &counter->col) ||
	    !checked_addition(counter->total_bytes, field_len, &counter->total_bytes)) {
		counter->overflow = true;
	}
	return;
}

static void cb_row_counter(int c, void *data) {
	(void)c;
	CsvCounter *counter = (CsvCounter *)data;
	if (counter->overflow) {
		return;
	}

	if (counter->row == 0) { // expected # cols based off first row
		counter->num_cols = counter->col;
	}

	if (counter->col != counter->num_cols) {
		counter->jagged_csv = true;
	}

	if (!checked_addition(counter->row, 1, &counter->row)) {
		counter->overflow = true;
		return;
	}
	counter->col = 0;

	counter->num_rows = counter->row;
	return;
}

typedef struct {
	char *data;
	size_t *offsets;
	size_t byte_cursor;
	size_t cell_index;
	size_t total_bytes;
	size_t total_cells;
	bool overflow;
} CsvCopier;

static void cb_field_copier(void *field, size_t field_len, void *data) {
	CsvCopier *copier = (CsvCopier *)data;
	size_t next_byte, next_cell;
	if (copier->overflow) {
		return;
	}
	if (!checked_addition(copier->byte_cursor, field_len, &next_byte) ||
	    !checked_addition(copier->cell_index, 1, &next_cell) ||
	    next_byte > copier->total_bytes || next_cell > copier->total_cells) {
		copier->overflow = true;
		return;
	}

	copier->offsets[copier->cell_index] = copier->byte_cursor;
	memcpy(copier->data + copier->byte_cursor, field, field_len);

	copier->byte_cursor = next_byte;
	copier->cell_index = next_cell;
	return;
}

CsvTableStatus csv_table_load(const char *path, bool has_header, Table *out_table) {
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
	csv_init(&parser, CSV_STRICT | CSV_STRICT_FINI);
	size_t parsed = csv_parse(&parser, csv_buffer, csv_size, cb_field_counter, cb_row_counter, &counter);
	if (parsed != csv_size) {
		int error = csv_error(&parser);
		fprintf(stderr, "CSV parsing (counting pass) aborted near byte %zu: %s\n", parsed, csv_strerror(error));
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}
	if (csv_fini(&parser, cb_field_counter, cb_row_counter, &counter) != 0) {
		int error = csv_error(&parser);
		fprintf(stderr, "CSV finalization (counting pass) failed: %s\n", csv_strerror(error));
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}
	if (counter.overflow) {
		fprintf(stderr, "CSV size overflow during counting pass\n");
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}

	if (counter.jagged_csv) {
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_JAGGED;
	}

	size_t total_cells, offset_count, offset_bytes, data_bytes;
	size_t col_lens_bytes, columns_bytes, rows_bytes;
	size_t header_rows = has_header && counter.num_rows > 0 ? 1 : 0;
	size_t data_rows = counter.num_rows - header_rows;
	if (!checked_multiply(counter.num_cols, counter.num_rows, &total_cells) ||
	    !checked_addition(total_cells, 1, &offset_count) ||
	    !checked_multiply(offset_count, sizeof(size_t), &offset_bytes) ||
	    !checked_addition(counter.total_bytes, 1, &data_bytes) ||
	    !checked_multiply(counter.num_cols, sizeof(size_t), &col_lens_bytes) ||
	    !checked_multiply(counter.num_cols, sizeof(Column), &columns_bytes) ||
	    !checked_multiply(data_rows, sizeof(char *), &rows_bytes)) {
		fprintf(stderr, "CSV size overflow while calculating allocations\n");
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}

	CsvCopier copier = {.total_bytes = counter.total_bytes, .total_cells = total_cells};

	copier.data = arena_alloc(&scratch, data_bytes, alignof(char));
	copier.offsets = arena_alloc(&scratch, offset_bytes, alignof(size_t));

	parsed = csv_parse(&parser, csv_buffer, csv_size, cb_field_copier, NULL, &copier);
	if (parsed != csv_size) {
		int error = csv_error(&parser);
		fprintf(stderr, "CSV parsing (copy pass) aborted near byte %zu: %s\n", parsed, csv_strerror(error));
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}
	if (csv_fini(&parser, cb_field_copier, NULL, &copier) != 0) {
		int error = csv_error(&parser);
		fprintf(stderr, "CSV finalization (copy pass) failed: %s\n", csv_strerror(error));
		csv_free(&parser);
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}
	csv_free(&parser);
	if (copier.overflow || copier.cell_index != total_cells ||
	    copier.byte_cursor != counter.total_bytes) {
		fprintf(stderr, "CSV size overflow or inconsistent copy pass\n");
		arena_term(&scratch);
		return CSV_TABLE_ERR_PARSE;
	}

	copier.data[counter.total_bytes] = '\0';
	copier.offsets[copier.cell_index] = copier.byte_cursor;

	size_t *col_lens = arena_zalloc(&scratch, col_lens_bytes, alignof(size_t));

	for (size_t i = 0; i < total_cells; i++) {
		size_t field_len = copier.offsets[i + 1] - copier.offsets[i];
		size_t terminated_len;
		if (!checked_addition(field_len, 1, &terminated_len) ||
		    !checked_addition(col_lens[i % counter.num_cols], terminated_len,
		                      &col_lens[i % counter.num_cols])) {
			fprintf(stderr, "CSV size overflow while calculating column lengths\n");
			arena_term(&scratch);
			return CSV_TABLE_ERR_PARSE;
		}
	}

	Table table = {0};
	arena_init(&table.arena);

	table.has_header = has_header;
	table.num_cols = counter.num_cols;
	table.num_rows = data_rows;
	table.columns = arena_alloc(&table.arena, columns_bytes, alignof(Column));

	for (size_t i = 0; i < table.num_cols; i++) {
		table.columns[i].name = NULL;
		table.columns[i].data = arena_alloc(&table.arena, col_lens[i], alignof(char));
		table.columns[i].rows = arena_alloc(&table.arena, rows_bytes, alignof(char *));
	}

	size_t *col_cursors = arena_zalloc(&scratch, col_lens_bytes, alignof(size_t));

	for (size_t i = 0; i < total_cells; i++) {
		size_t row = i / counter.num_cols;
		size_t col = i % counter.num_cols;
		size_t field_len = copier.offsets[i + 1] - copier.offsets[i];
		size_t terminated_len, next_cursor;
		if (!checked_addition(field_len, 1, &terminated_len) ||
		    !checked_addition(col_cursors[col], terminated_len, &next_cursor) ||
		    next_cursor > col_lens[col]) {
			fprintf(stderr, "CSV size overflow while copying columns\n");
			arena_term(&table.arena);
			arena_term(&scratch);
			return CSV_TABLE_ERR_PARSE;
		}

		char *dest = table.columns[col].data + col_cursors[col];
		memcpy(dest, &copier.data[copier.offsets[i]], field_len);
		dest[field_len] = '\0';

		if (has_header && row == 0) {
			table.columns[col].name = dest;
		} else {
			table.columns[col].rows[row - header_rows] = dest;
		}
		col_cursors[col] = next_cursor;
	}

	arena_term(&scratch);

	*out_table = table;
	return CSV_TABLE_OK;
}

void csv_table_free(Table *table) {
	arena_term(&table->arena);
	return;
}
