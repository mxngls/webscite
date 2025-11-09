#ifndef HTML_H
#define HTML_H

#include "page.h"

#ifndef _SITE_EXT_TITLE
#define _SITE_EXT_TITLE "SITE_TITLE"
#endif

#define _SITE_BLOCK_DIR_PATH   _SITE_EXT_SOURCE_DIR "/blocks"
#define _SITE_STYLE_SHEET_PATH "/style.css"

// clang-format off
#define _SITE_HTML_FONT \
	"<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n" \
	"<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\n" \
	"<link href=\"https://fonts.googleapis.com/css2?family=Inconsolata:wdth,wght@95.3,200..900&family=Roboto+Flex:opsz,wght@8..144,100..1000&display=swap\" rel=\"stylesheet\">\n"
// clang-format on

#define _SITE_SCRIPT "<script src=\"/script.js\" defer></script>"

typedef struct {
        long len;
        char *content;
} htm_block;

// initialize templates
int html_init_templates(void);
void html_cleanup_templates(void);

// create html files
int html_create_page(page_header *, char *, char *);
int html_create_index(char *, char *, page_header_arr *, const char *[], int);
char *html_escape_content(char *);

#endif // HTML_H
