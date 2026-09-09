#include <stdio.h>

#include "csv_table.h"

int main(void) {
	const char* csv_path = "./resources/sample_large.csv";
	Table table;
	CsvTableStatus stat = csv_table_load(csv_path, &table);

	switch (stat) {
		case CSV_TABLE_ERR_FILE:
			fprintf(stderr, "error: could not read CSV file\n");
			return 1;
		case CSV_TABLE_ERR_JAGGED:
			fprintf(stderr, "error: jagged CSV\n");
			return 2;
		case CSV_TABLE_ERR_PARSE:
			fprintf(stderr, "error: couldn't parse CSV\n");
			return 3;
		case CSV_TABLE_OK:
			break;
	}

	printf("successfully created table from %s\n", csv_path);

	csv_table_free(&table);
	return 0;
}
