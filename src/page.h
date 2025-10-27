#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdio.h>

#define _SITE_PAGES_MAX 50
#define _SITE_PATH_MAX  100

typedef struct {
        char *title;
        char *subtitle;
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

// work with page headers
int page_parse_header(FILE *, page_header *);

#endif // PAGE_H
