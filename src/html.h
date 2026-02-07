#ifndef HTML_H
#define HTML_H

#include <stdbool.h>

#include "page.h"

#define _SITE_BLOCK_DIR_PATH	     _SITE_EXT_SOURCE_DIR "/blocks"
#define _SITE_STYLE_SHEET_PATH	     "/style.css"
#define _SITE_RESET_STYLE_SHEET_PATH "/reset.css"

typedef struct {
	long len;
	char* content;
	char* hash;
} htm_block;

// initialize templates
int html_init_templates(void);
void html_cleanup_templates(void);

// create html files
int html_create_page(page_header*, char*, char*, page_header_arr*, bool, bool, bool, bool);
char* html_escape_content(char*);

#endif // HTML_H
