#ifndef CSV_TABLE_H
#define CSV_TABLE_H

#include <stddef.h>

#include "arena.h"

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

typedef enum {
	CSV_TABLE_OK = 0,
	CSV_TABLE_ERR_FILE,
	CSV_TABLE_ERR_JAGGED,
} CsvTableError;

CsvTableError csv_table_load(const char *path, Table *out_table);
void csv_table_free(Table *table);

#endif // CSV_TABLE_H
