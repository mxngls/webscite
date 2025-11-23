#ifndef HTML_H
#define HTML_H

#include <stdbool.h>

#include "page.h"

#define _SITE_BLOCK_DIR_PATH	     _SITE_EXT_SOURCE_DIR "/blocks"
#define _SITE_STYLE_SHEET_PATH	     "/style.css"
#define _SITE_RESET_STYLE_SHEET_PATH "/reset.css"

// clang-format off
#define _SITE_HTML_FONT \
	"<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n" \
	"<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\n" \
	"<link href=\"https://fonts.googleapis.com/css2?family=Source+Sans+3:ital,wght@0,200..900;1,200..900&display=swap\" rel=\"stylesheet\">\n" // clang-format on

#define _SITE_SCRIPT "<script src=\"/script.js\" defer></script>"

typedef struct {
	long len;
	char* content;
} htm_block;

// initialize templates
int html_init_templates(void);
void html_cleanup_templates(void);

// create html files
int html_create_page(
    page_header*,
    char*,
    char*,
    page_header_arr*,
    const char*[],
    int,
    bool,
    bool,
    bool);
char* html_escape_content(char*);

#endif // HTML_H
