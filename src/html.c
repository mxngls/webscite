#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "error.h"
#include "ghist.h"
#include "html.h"
#include "page.h"

// global template content
static char* site_head = NULL;
static char* site_header = NULL;
static char* site_footer = NULL;

// shared template building blocks
static int __html_parse_block(const char* block_path, htm_block* block)
{
	FILE* block_file = NULL;
	char* block_content = NULL;
	int res = -1;

	// open file
	block_file = fopen(block_path, "r");
	if (block_file == NULL) {
		ERRORF(SITE_ERROR_FILE_OPEN_READ, block_path);
		goto cleanup;
	}

	if (fseek(block_file, 0, SEEK_END) != 0) {
		ERRORF(SITE_ERROR_FILE_SEEK, block_path);
		goto cleanup;
	}

	long file_size = ftell(block_file);
	if (file_size < 0) {
		ERRORF(SITE_ERROR_FILE_TELL, block_path);
		goto cleanup;
	}
	rewind(block_file);

	// allocate buffer
	block_content = malloc(file_size + 1);
	if (block_content == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		goto cleanup;
	}

	// read entire file
	size_t bytes_read = fread(block_content, 1, file_size, block_file);
	if (bytes_read != (size_t)file_size) {
		ERRORF(SITE_ERROR_FILE_READ, block_path);
		goto cleanup;
	}

	block_content[bytes_read] = '\0';

	printf("%s\n\n\n", block_content);

	// success - transfer ownership to caller
	block->content = block_content;
	block->len = bytes_read;
	block_content = NULL; // don't free on cleanup
	res = 0;

cleanup:
	if (block_file)
		fclose(block_file);
	if (block_content)
		free(block_content);

	return res;
}

// initialize all templates
int html_init_templates(void)
{
	// (1) parse block content
	// (2) transfer ownership to global variable

	htm_block head_block = { 0 };
	htm_block header_block = { 0 };
	htm_block footer_block = { 0 };

	// load head (required)
	if (access(_SITE_BLOCK_DIR_PATH "/head.htm", F_OK) == 0) {
		if (__html_parse_block(_SITE_BLOCK_DIR_PATH "/head.htm", &head_block) != 0) {
			goto error;
		}
	}

	// load header (optional)
	if (access(_SITE_BLOCK_DIR_PATH "/header.htm", F_OK) == 0) {
		if (__html_parse_block(_SITE_BLOCK_DIR_PATH "/header.htm", &header_block) != 0) {
			goto error;
		}
	}

	// load footer (optional)
	if (access(_SITE_BLOCK_DIR_PATH "/footer.htm", F_OK) == 0) {
		if (__html_parse_block(_SITE_BLOCK_DIR_PATH "/footer.htm", &footer_block) != 0) {
			goto error;
		}
	}

	site_head = head_block.content;
	site_header = header_block.content;
	site_footer = footer_block.content;

	return 0;

error:
	if (head_block.content)
		free(head_block.content);
	if (header_block.content)
		free(header_block.content);
	if (footer_block.content)
		free(footer_block.content);
	return -1;
}

// cleanup templates
void html_cleanup_templates(void)
{
	if (site_head) {
		free(site_head);
		site_head = NULL;
	}
	if (site_header) {
		free(site_header);
		site_header = NULL;
	}
	if (site_footer) {
		free(site_footer);
		site_footer = NULL;
	}
}

// create plain html file
int html_create_page(page_entry* entry, char* plain_content, char* output_path)
{
	int res = 0;

	char* escaped_title = NULL;
	char* escaped_description = NULL;

	// html destination
	FILE* dest_file = fopen(output_path, "w");
	if (dest_file == NULL) {
		ERRORF(SITE_ERROR_FILE_CREATE, output_path);
		goto error;
	}

	int fprintf_ret = 0;

	if (html_escape_content(entry->headers.title, &escaped_title)) {
		goto error;
	}
	if (html_escape_content(entry->headers.description, &escaped_description)) {
		goto error;
	};

	// page title (and possibly it's description)
	fprintf_ret = fprintf(
	    dest_file,
	    "<!DOCTYPE html>"
	    "<html lang=\"en\">"
	    "<head>"
	    "<title>%s</title>"
	    "%s%s%s",
	    escaped_title,

	    escaped_description ? "<meta name=\"description\"content=\"" : "",
	    escaped_description ? escaped_description : "", escaped_description ? "\">" : "");

	fprintf_ret = fprintf(
	    dest_file,

	    "<link href=\"/feed.atom\"type=\"application/atom+xml\"rel=\"alternate\"/>"
	    "%s"     // default style sheet
	    "%s%s%s" // custom style sheet
	    "%s"     // custom head content
	    "</head>",

	    entry->headers.include_styles
		? "<link rel=\"stylesheet\"href=\"/style.css\"type=\"text/css\">"
		: "",

	    entry->headers.stylesheet ? "<link rel=\"stylesheet\"href=\"" : "",
	    entry->headers.stylesheet ? entry->headers.stylesheet : "",
	    entry->headers.stylesheet ? "\"type=\"text/css\">" : "",

	    site_head);

	// wrapper class(es)
	fprintf_ret = fprintf(
	    dest_file,
	    "<body>"
	    "<div id=\"wrap\"class=\"%s%s%s\">",
	    entry->headers.is_post ? "post" : "",

	    entry->headers.class ? "" : "", entry->headers.class ? entry->headers.class : "");

	// header tag
	fprintf_ret = fprintf(
	    dest_file,
	    "%s"
	    "<main>",
	    entry->headers.include_header && site_header ? site_header : "");

	if (entry->headers.is_post) {
		fprintf_ret = fprintf(dest_file, "<article>");
	}

	size_t buf_size = 48 * 1024;
	char* buf = NULL;
	if ((buf = malloc(buf_size)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION)
		goto error;
	}

	// title?
	if (entry->headers.include_title) {
		fprintf_ret = fprintf(dest_file, "<h1>%s</h1>", entry->headers.title);
	}

	// date(s)?
	if (entry->headers.include_date) {
		char created_date[256];
		char created_formatted_date[256];
		if (entry->meta.created) {
			ghist_format_ts("%Y-%m-%d", created_date, entry->meta.created);
			ghist_format_ts("%b %m, %Y", created_formatted_date, entry->meta.modified);
		} else {
			snprintf(
			    created_formatted_date, sizeof(created_formatted_date), "%s", "DRAFT");
		}

		// add updated date at the end if present
		int has_modified = entry->meta.modified != 0;
		if (has_modified) {
			char modified_date[256];
			char modified_formatted_date[256];
			ghist_format_ts("%Y-%m-%d", modified_date, entry->meta.modified);
			ghist_format_ts("%b %m, %Y", modified_formatted_date, entry->meta.modified);
			fprintf_ret = fprintf(
			    dest_file,
			    "<div id=\"post-date\">"
			    "<div id=\"date-created\">"
			    "<time datetime=\"%s\">%s</time>"
			    "</div>"
			    "<div id=\"date-updated\">"
			    "<time datetime=\"%s\">%s</time>"
			    "</div>"
			    "</div>",
			    created_date, created_formatted_date, modified_date,
			    modified_formatted_date);
		} else {
			fprintf_ret = fprintf(
			    dest_file,
			    "<div id=\"post-date\">"
			    "<div id=\"date-created\">"
			    "<time>%s</time>"
			    "</div>"
			    "</div>",
			    created_formatted_date);
		}
	}

	// write content
	fprintf_ret = fprintf(dest_file, "%s", plain_content);

	if (entry->headers.is_post) {
		fprintf_ret = fprintf(dest_file, "</article>");
	}

	// close html
	fprintf_ret = fprintf(
	    dest_file,
	    "</main>"
	    "%s"
	    "</div>"
	    "</body>"
	    "</html>",
	    entry->headers.include_footer && site_footer ? site_footer : "");

	if (fprintf_ret < 0) {
		ERRORF(SITE_ERROR_FILE_WRITE, dest_file);
		goto error;
	}

	goto cleanup;

error:
	res = -1;

cleanup:
	if (dest_file) {
		fclose(dest_file);
	}

	free(escaped_title);
	free(escaped_description);

	return res;
}

// escape html entities
int html_escape_content(char* html_content, char** escaped_content)
{
	if (html_content == NULL) {
		*escaped_content = NULL;
		return 0;
	}

	// first calculate exact size needed
	static const char* const entities[256] = {
		// clang-format off
        ['"'] = "&quot;",
        ['\''] = "&#39;",
        ['&'] = "&amp;",
        ['<'] = "&lt;",
        ['>'] = "&gt;",
		// clang-format on
	};

	size_t need = 1;
	for (char* cp = html_content; *cp; cp++) {
		const char* ent = entities[(unsigned char)*cp];
		size_t add = ent ? strlen(ent) : 1;
		if (need > SIZE_MAX - add) {
			ERROR(SITE_ERROR_MEMORY_ALLOCATION);
			return -1;
		}
		need += add;
	}

	char* out = malloc(need);
	if (!out) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		return -1;
	}

	char* write = out;
	for (char* p = html_content; *p; p++) {
		const char* ent = entities[(unsigned char)*p];
		if (ent) {
			size_t ent_len = strlen(ent);
			memcpy(write, ent, ent_len);
			write += ent_len;
		} else {
			*write++ = *p;
		}
	}
	*write = '\0';
	*escaped_content = out;

	return 0;
}
