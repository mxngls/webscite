#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdio.h>

#define _SITE_PAGES_MAX 50
#define _SITE_PATH_MAX	100

typedef enum {
	PAGE_KIND_POST,
	PAGE_KIND_LINK,
} page_kind;

typedef struct {
	page_kind kind;
	char* title;
	union {
		char* link;
		char* content;
	};
	struct {
		char path[_SITE_PATH_MAX];
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
