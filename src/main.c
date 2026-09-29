#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "csv_table.h"
#include "file.h"

int main(int argc, char *argv[]) {
	const char* csv_path = NULL;
	bool has_header = true;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--csv") == 0) {
			if (i + 1 >= argc || strncmp(argv[i + 1], "--", 2) == 0) {
				fprintf(stderr, "--csv requires a path argument\n");
				return 2;
			}

			csv_path = argv[++i];
			if (!file_is_regular(argv[i])) {
				fprintf(stderr, "--csv argument is an invalid path\n");
				return 3;
			}
			continue;
		}
		if (strcmp(argv[i], "--no-header") == 0) {
			has_header = false;
			continue;
		}
		fprintf(stderr, "unknown argument: %s\n", argv[i]);
		return 4;
	}

	if (csv_path == NULL) {
		fprintf(stderr, "Usage: %s --csv PATH [--no-header]\n", argv[0]);
		return 5;
	}

	Table table;
	CsvTableStatus stat = csv_table_load(csv_path, has_header, &table);

	switch (stat) {
		case CSV_TABLE_ERR_FILE:
			fprintf(stderr, "error: could not read CSV file\n");
			return 5;
		case CSV_TABLE_ERR_JAGGED:
			fprintf(stderr, "error: jagged CSV\n");
			return 6;
		case CSV_TABLE_ERR_PARSE:
			fprintf(stderr, "error: couldn't parse CSV\n");
			return 7;
		case CSV_TABLE_OK:
			break;
	}

	printf("successfully created table from %s\n", csv_path);

	csv_table_free(&table);
	return 0;
}
