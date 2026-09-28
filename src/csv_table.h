#ifndef CSV_TABLE_H
#define CSV_TABLE_H

#include <stddef.h>
#include <stdbool.h>

#include "arena.h"

typedef struct {
	char *data;
	char **rows;
	char *name;
} Column;

typedef struct {
	Column *columns;
	Arena arena;
	size_t num_cols;
	size_t num_rows;
	bool has_header;
} Table;

typedef enum {
	CSV_TABLE_OK = 0,
	CSV_TABLE_ERR_FILE,
	CSV_TABLE_ERR_JAGGED,
	CSV_TABLE_ERR_PARSE
} CsvTableStatus;

CsvTableStatus csv_table_load(const char *path, bool has_header, Table *out_table);
void csv_table_free(Table *table);

#endif // CSV_TABLE_H
