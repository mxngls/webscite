#ifndef HTML_H
#define HTML_H

#include <stdbool.h>

#include "page.h"

#define _SITE_BLOCK_DIR_PATH _SITE_EXT_SOURCE_DIR "/blocks"

typedef struct {
	long len;
	char* content;
} htm_block;

// initialize templates
int html_init_templates(void);
void html_cleanup_templates(void);

// create html files
int html_create_page(page_entry*, char*, char*);
int html_escape_content(char*, char**);

#endif // HTML_H
