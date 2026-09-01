#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdalign.h>

#include "file.h"

long file_get_size(const char *path) {
	struct stat st;

	if (stat(path, &st) != 0) {
		fprintf(stderr, "error getting file size using stat: %s\n", path);
		return -1;
	}

	return st.st_size;
}

bool file_exists(const char *path) {
	struct stat st;

	if (stat(path, &st) == 0) {
		return true;
	}
	return false;
}

char *file_read_all(Arena *arena, const char *path, size_t *out_size) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "error opening file to read: %s\n", path);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	if (size == -1) {
		fprintf(stderr, "error getting file size\n");
		fclose(fp);
		return NULL;
	}
	char *output = arena_alloc(arena, size + 1, alignof(char));
	fseek(fp, 0, SEEK_SET);

	size_t ret = fread(output, 1, size, fp);
	if (ret != (size_t)size) {
		fprintf(stderr, "error reading file contents\n");
		fclose(fp);
		return NULL;
	}
	output[size] = '\0';
	fclose(fp);

	if (out_size) {
		*out_size = (size_t)size;
	}

	return output;
}

int file_write_all(const char *data, const char *path) {
	FILE *fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "error opening file to write: %s\n", path);
		return 1;
	}

	size_t write_len = strlen(data);

	size_t ret = fwrite(data, 1, write_len, fp);
	if (ret != write_len) {
		fprintf(stderr, "error writing file contents\n");
		fclose(fp);
		return 2;
	}

	fclose(fp);
	return 0;
}

int file_append(const char *data, const char *path) {
	FILE *fp = fopen(path, "a");
	if (!fp) {
		fprintf(stderr, "error opening file (%s) to append\n", path);
		return 1;
	}

	size_t write_len = strlen(data);
	size_t ret = fwrite(data, 1, write_len, fp);
	if (ret != write_len) {
		fprintf(stderr, "error appending contents to file\n");
		fclose(fp);
		return 2;
	}

	fclose(fp);
	return 0;
}
