#ifndef FILE_H
#define FILE_H

#include <stdbool.h>

#include "arena.h"

long file_get_size(const char *path);
bool file_exists(const char *path);
char *file_read_all(Arena *arena, const char *path, size_t *out_size);
int file_write_all(const char *data, const char *path);
int file_append(const char *data, const char *path);

#endif // FILE_H
