#ifndef PAGE_H
#define PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define _SITE_PAGES_MAX 50
#define _SITE_PATH_MAX	100

typedef struct {
	char* title;
	char* content;
	bool is_post;
	struct {
		bool header;
		bool footer;
		bool title;
		bool date;
	} includes;
	struct {
		char path[_SITE_PATH_MAX];
		char source_path[_SITE_PATH_MAX];
		char hash[8]; // NOTE: currently unused
		int64_t created;
		int64_t modified;
	} meta;
} page_entry;

typedef struct {
	page_entry* elems[_SITE_PAGES_MAX];
	int len;
} page_entry_arr;

// work with pages
int page_parse_header(FILE*, page_entry*);
int page_parse_content(FILE*, char*, size_t, char*);

#endif // PAGE_H
