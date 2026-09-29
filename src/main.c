#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "csv_table.h"

int main(int argc, char *argv[]) {
	const char* csv_path = NULL;
	bool has_header = true;

	if (argc == 1) {
		fprintf(stderr, "arguments missing\n");
		return 1;
	}

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--csv") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "--csv requires a path argument\n");
				return 1;
			}
			csv_path = argv[i];
			continue;
		}
		if (strcmp(argv[i], "--no-header") == 0) {
			has_header = false;
			continue;
		}
		fprintf(stderr, "unknown argument: %s\n", argv[i]);
		return 1;
	}

	Table table;
	CsvTableStatus stat = csv_table_load(csv_path, has_header, &table);

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
