#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdio.h>

#define _SITE_PAGES_MAX 50
#define _SITE_PATH_MAX  100

typedef struct {
        char *title;
        struct {
                char path[_SITE_PATH_MAX];
                int64_t created;
                int64_t modified;
        } meta;
} page_header;

typedef struct {
        page_header *elems[_SITE_PAGES_MAX];
        int len;
} page_header_arr;

typedef struct {
        char *content;
        struct {
                char path[_SITE_PATH_MAX];
        } meta;
} page_content;

typedef struct {
        char *elems[_SITE_PAGES_MAX];
        int len;
} page_content_arr;

extern page_content_arr content_arr;

typedef struct {
        page_header *header;
        char *content;
} page_result;

// work with pages
int page_parse_header(FILE *, page_header *);
int page_parse_content(FILE *, char *, size_t, char *);

#endif // PAGE_H
