#ifndef PAGE_H
#define PAGE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define _SITE_PAGES_MAX 50

typedef struct {
	struct {
		char* title;
		char* class;
		bool is_post;
		bool include_header;
		bool include_footer;
		bool include_title;
		bool include_date;
	} headers;
	struct {
		char path[PATH_MAX];
		char source_path[PATH_MAX];
		char hash[8]; // NOTE: currently unused
		int64_t created;
		int64_t modified;
	} meta;
	char* content;
} page_entry;

typedef struct {
	page_entry* elems[_SITE_PAGES_MAX];
	int len;
} page_entry_arr;

// work with pages
int page_parse_header(FILE*, page_entry*);
int page_parse_content(FILE*, char*, size_t, char*);

#endif // PAGE_H
