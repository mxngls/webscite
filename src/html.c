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
char* site_head = NULL;
char* site_header = NULL;
char* site_footer = NULL;

// style sheet and script blob hashes
char style_sheet_hash[8] = { '\0' };
char reset_sheet_hash[8] = { '\0' };
char script_hash[8] = { '\0' };

// shared template building blocks
__attribute__((unused)) static int __html_parse_block(const char* block_path, htm_block* block)
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

	// get file size using fseek/ftell
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
	if (__html_parse_block(_SITE_BLOCK_DIR_PATH "/head.htm", &head_block) != 0) {
		goto error;
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

// package content
static char* __html_create_content(page_entry* entry, char* page_content, char* additional_content)
{
	bool include_title = entry->includes.title;
	bool include_date = entry->includes.date;

	size_t buf_size = 48 * 1024;
	char* buf = NULL;
	if ((buf = malloc(buf_size)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION)
		return NULL;
	}

	char* pos = buf;
	int offset = 0;

	offset = snprintf(pos, buf_size - (pos - buf), "%s\n", "");
	pos += offset;

	if (include_title) {
		offset = snprintf(pos, buf_size - (pos - buf), "<h1>%s</h1>\n", entry->title);
		pos += offset;
	}

	if (include_date) {
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
			offset = snprintf(
			    pos, buf_size - (pos - buf),
			    // clang-format off
                                  "<div id=\"post-date\">\n"
                                      "<div id=\"date-created\">\n"
                                          "<time datetime=\"%s\">%s</time>\n"
                                      "</div>\n"
                                      "<div id=\"date-updated\">\n"
                                          "<time datetime=\"%s\">%s</time>\n"
                                      "</div>\n"
                                  "</div>\n",
			    // clang-format on
			    created_date, created_formatted_date, modified_date,
			    modified_formatted_date);
			pos += offset;
		} else {
			offset = snprintf(
			    pos, buf_size - (pos - buf),
			    // clang-format off
                                  "<div id=\"post-date\">\n"
                                      "<div id=\"date-created\">\n"
                                          "%s\n"
                                      "</div>\n"
                                  "</div>\n",
			    // clang-format on
			    created_formatted_date);
			pos += offset;
		}
	}

	// add content
	offset = snprintf(pos, buf_size - (pos - buf), "%s", page_content);
	pos += offset;

	// add additional content if provided
	if (additional_content) {
		offset = snprintf(pos, buf_size - (pos - buf), "%s", additional_content);
		pos += offset;
	}

	// close main content
	offset = snprintf(pos, buf_size - (pos - buf), "%s\n", "");
	pos += offset;

	return buf;
}

// create plain html file
int html_create_page(page_entry* entry, char* plain_content, char* output_path)
{
	int res = 0;

	// html destination
	FILE* dest_file = fopen(output_path, "w");
	if (dest_file == NULL) {
		ERRORF(SITE_ERROR_FILE_CREATE, output_path);
		goto error;
	}

	int fprintf_ret = 0;

	// append abbreviated Git blob hashes to circumvent overly aggressive
	// browser cashing (Safari)
	if (style_sheet_hash[0] == '\0' || reset_sheet_hash[0] == '\0' || script_hash[0] == '\0') {
		char style_sheet_path[] = _SITE_EXT_SOURCE_DIR "/style.css";
		if (ghist_blob_hash(style_sheet_hash, sizeof(style_sheet_hash), style_sheet_path)) {
			goto error;
		}
		char reset_sheet_path[] = _SITE_EXT_SOURCE_DIR "/reset.css";
		if (ghist_blob_hash(reset_sheet_hash, sizeof(reset_sheet_hash), reset_sheet_path)) {
			goto error;
		}
		char script_path[] = _SITE_EXT_SOURCE_DIR "/script.js";
		if (ghist_blob_hash(script_hash, sizeof(script_hash), script_path)) {
			goto error;
		}
	}

	fprintf_ret = fprintf(
	    dest_file,
	    // clang-format off
            "<!DOCTYPE html>"
            "<html lang=\"en\">\n"
            "<head>\n"
            "%s"
            "    <title>%s</title>\n"
			"    <link rel=\"stylesheet\" href=\"/reset.css?=%s\" type=\"text/css\">\n"
	    	"    <link rel=\"stylesheet\" href=\"/style.css?=%s\" type=\"text/css\">\n"
			"    <script src=\"/script.js?=%s\" defer></script>\n"
        	"</head>\n"
        	"<body>\n"
			"    <div id=\"wrap\" class=\"%s\">\n"
	    	"        %s"
            "        <main>\n",
	    // clang-format on
	    site_head, entry->title, reset_sheet_hash, style_sheet_hash, script_hash,
	    entry->is_post ? "post" : "non-post",
	    entry->includes.header && site_header ? site_header : "");

	// write content
	char* html_content = NULL;
	if ((html_content = __html_create_content(
		 entry, plain_content,
		 NULL // no additional content yet so we pass NULL
		 ))
	    == NULL) {
		goto error;
	}

	if (entry->is_post) {
		fprintf_ret = fprintf(
		    dest_file,
		    "    <article>\n"
		    "        %s\n"
		    "    </article>\n",
		    html_content);
	} else {
		fprintf_ret = fprintf(dest_file, "%s\n", html_content);
	}

	// close html
	fprintf_ret = fprintf(
	    dest_file,
	    // clang-format off
            "        </main>\n"
            "        %s"
            "    </div>\n"
            "</body>\n"
            "</html>\n",
	    // clang-format on
	    entry->includes.footer && site_footer ? site_footer : "");

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

	return res;
}

// escape html entities
char* html_escape_content(char* html_content)
{
	// first calculate exact size needed
	static const char* const entities[256] = {
		// clang-format off
        ['"'] = "&quot;",
        ['\''] = "&#39;",
        ['&'] = "&amp;",
        ['<'] = "&lt;",
        ['>'] = "&gt",
		// clang-format on
	};

	size_t need = 1;
	for (char* cp = html_content; *cp; cp++) {
		const char* ent = entities[(unsigned char)*cp];
		size_t add = ent ? strlen(ent) : 1;
		if (need > SIZE_MAX - add) {
			return NULL;
		}
		need += add;
	}

	char* out = malloc(need);
	if (!out) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		return NULL;
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

	return out;
}
