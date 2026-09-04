#include <stdio.h>

#include "csv_table.h"

int main(void) {
	Table table;
	CsvTableError err = csv_table_load("./resources/sample_jagged.csv", &table);

	switch (err) {
	case CSV_TABLE_ERR_FILE:
		fprintf(stderr, "error: could not read CSV file\n");
		return 1;
	case CSV_TABLE_ERR_JAGGED:
		fprintf(stderr, "error: jagged CSV\n");
		return 2;
	case CSV_TABLE_OK:
		break;
	}

	for (size_t row = 0; row < table.num_rows; row++) {
		for (size_t col = 0; col < table.num_cols; col++) {
			printf("%s ", table.columns[col].rows[row]);
		}
		printf("\n");
	}

	csv_table_free(&table);
	return 0;
}
