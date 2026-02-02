#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct LineRow {
	uint32_t start;
	uint32_t end;
	uint32_t file;
	uint32_t line;
} LineRow;

typedef struct LineTable {
	char **dirs;
	size_t ndirs;
	char **files;
	uint32_t *file_dir;
	size_t nfiles;
	LineRow *rows;
	size_t nrows;
} LineTable;

int
geo_elf_load_line_table(const char *elf_path, LineTable *out);

void
geo_elf_free_line_table(LineTable *lt);

static inline const LineRow *
geo_line_find_row_addr(const LineRow *rows, size_t nrows, uint32_t addr)
{
	size_t lo = 0;
	size_t hi = nrows;
	while (lo < hi) {
		size_t mid = lo + ((hi - lo) >> 1);
		if (rows[mid].start <= addr) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	if (lo == 0) {
		return NULL;
	}
	const LineRow *r = &rows[lo - 1];
	if (addr >= r->start && addr < r->end) {
		return r;
	}
	return NULL;
}
