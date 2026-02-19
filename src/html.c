#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
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

// style sheet blob hashes
char style_sheet_hash[8] = { '\0' };
char reset_sheet_hash[8] = { '\0' };

// compare by creation time
static int __qsort_cb(const void* a, const void* b)
{
	page_header* header_a = *(page_header**)a;
	page_header* header_b = *(page_header**)b;

	// descending order (newest first)
	if (header_a->meta.created > header_b->meta.created)
		return -1;
	if (header_a->meta.created < header_b->meta.created)
		return 1;
	return 0;
}

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
static char* __html_create_content(
    page_header* header,
    char* page_content,
    char* additional_content,
    bool include_title,
    bool include_date)
{

	size_t buf_size = 48 * 1024;
	char* buf = NULL;
	if ((buf = malloc(buf_size)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION)
		return NULL;
	}

	char* pos = buf;
	int offset = 0;

	offset = snprintf(pos, buf_size - (pos - buf), "%s\n", "<div id=\"post-body\">");
	pos += offset;

	if (include_title) {
		offset = snprintf(pos, buf_size - (pos - buf), "<h1>%s</h1>\n", header->title);
		pos += offset;
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
	offset = snprintf(pos, buf_size - (pos - buf), "%s\n", "</div>");
	pos += offset;

	if (include_date) {
		char created_date[256];
		char created_formatted_date[256];
		if (header->meta.created) {
			ghist_format_ts("%Y-%m-%d", created_date, header->meta.created);
			snprintf(
			    created_formatted_date, sizeof(created_formatted_date), "Created on %s",
			    created_date);
		} else {
			snprintf(
			    created_formatted_date, sizeof(created_formatted_date), "%s", "DRAFT");
		}

		// add updated date at the end if present
		int has_modified = header->meta.modified != 0;
		if (has_modified) {
			char modified_date[256];
			char modified_formatted_date[256];
			ghist_format_ts("%Y-%m-%d", modified_date, header->meta.modified);
			snprintf(
			    modified_formatted_date, sizeof(modified_formatted_date),
			    "Last updated on %s", modified_date);
			offset = snprintf(
			    pos, buf_size - (pos - buf),
			    // clang-format off
                                  "<div id=\"post-date\">\n"
                                      "<div id=\"date-created\">\n"
                                          "%s\n"
                                      "</div>\n"
                                      "<div id=\"date-updated\">\n"
                                          "%s\n"
                                      "</div>\n"
                                  "</div>\n",
			    // clang-format on
			    created_formatted_date, modified_formatted_date);
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

	return buf;
}

// create HTML list of all posts
static char* __html_post_list(page_header_arr* header_arr)
{

	size_t buf_size = 48 * 1024;
	char* buf = NULL;
	if ((buf = malloc(buf_size)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		return NULL;
	}

	// sort by creation time
	qsort(header_arr->elems, header_arr->len, sizeof(page_header*), __qsort_cb);

	char* pos = buf;
	int offset = 0;

	// add a list of posts to the index
	offset = snprintf(
	    pos, buf_size - (pos - buf),
	    "<section id=\"post-list\">\n"
	    "    <ul>\n");
	pos += offset;

	for (int i = 0; i < header_arr->len; i++) {
		char created_date[256];
		if (header_arr->elems[i]->meta.created) {
			ghist_format_ts("%Y", created_date, header_arr->elems[i]->meta.created);
		} else {
			snprintf(created_date, sizeof(created_date), "%s", "DRAFT");
		}

		offset = snprintf(
		    pos, buf_size - (pos - buf),
		    // clang-format off
			"<li>\n"
				"<a href=\"%s\">\n"
					"<span class=\"post-list-date\">%s</span>\n"
					"<span class=\"post-list-title\">%s</span>\n"
				"</a>\n"
        		"</li>\n",
		    // clang-format on
		    header_arr->elems[i]->meta.path, created_date, header_arr->elems[i]->title);
		pos += offset;
	}

	offset = snprintf(
	    pos, buf_size - (pos - buf),
	    "    </ul>\n"
	    "</section>\n");
	pos += offset;

	return buf;
}

// create plain html file
int html_create_page(
    page_header* header,
    char* plain_content,
    char* output_path,
    page_header_arr* header_arr,
    bool is_blog,
    bool include_back_ref,
    bool include_title,
    bool include_date)
{
	int res = 0;

	// html destination
	FILE* dest_file = fopen(output_path, "w");
	if (dest_file == NULL) {
		ERRORF(SITE_ERROR_FILE_CREATE, output_path);
		goto error;
	}

	int fprintf_ret = 0;

	// if blog then add post list
	char* post_list = NULL;
	if (is_blog) {
		if ((post_list = __html_post_list(header_arr)) == NULL) {
			goto error;
		}
	};

	// append abbreviated Git blob hashes to circumvent overly aggressive browser cashing
	// (Safari)
	if (style_sheet_hash[0] == '\0' || reset_sheet_hash[0] == '\0') {
		char style_sheet_path[] = _SITE_EXT_SOURCE_DIR "/style.css";
		if (ghist_blob_hash(style_sheet_hash, sizeof(style_sheet_hash), style_sheet_path)) {
			goto error;
		}
		char reset_sheet_path[] = _SITE_EXT_SOURCE_DIR "/reset.css";
		if (ghist_blob_hash(reset_sheet_hash, sizeof(reset_sheet_hash), reset_sheet_path)) {
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
            "</head>\n"
            "<body>\n"
	    "    <div id=\"content\">\n"
	    "    %s"
            "    <main>\n",
	    // clang-format on
	    site_head, header->title, reset_sheet_hash, style_sheet_hash,
	    site_header ? site_header : "");

	if (include_back_ref) {
		char blog_main[_SITE_PATH_MAX] = "";
		snprintf(
		    blog_main, sizeof(blog_main), "/%sl", // append "l" to existing ".htm" extension
		    _SITE_EXT_BLOG_INDEX);
		fprintf_ret = fprintf(
		    dest_file, "    <span id=\"post-back-ref\"><a  href=\"%s\">back</a></span>\n",
		    blog_main);
	}

	// write content
	char* html_content = NULL;
	if ((html_content
	     = __html_create_content(header, plain_content, post_list, include_title, include_date))
	    == NULL) {
		goto error;
	}

	fprintf_ret = fprintf(
	    dest_file,
	    // clang-format off
	    "    <article id=\"post\">\n"
	    "        %s\n" 
	    "    </article>\n",
	    // clang-format on
	    html_content);

	// close html
	fprintf_ret = fprintf(
	    dest_file,
	    // clang-format off
            "        </main>\n"
            "        %s\n"
            "    </div>\n"
            "</body>\n"
            "</html>\n",
	    // clang-format on
	    is_blog && site_footer ? site_footer : "");

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
	int content_size = 0;
	char* html_content_copy = html_content;
	while (*html_content_copy++) {
		content_size++;
	}

	// conservative estimate of size overhead due to necessary escaping
	unsigned long escaped_size = (unsigned long)(content_size * 2);
	char* escaped = malloc(escaped_size);
	if (escaped == NULL) {
		return NULL;
	}
	escaped[0] = '\0';

	char* pos = escaped;

	while (*html_content && escaped_size - (pos - escaped) > 0) {
		size_t buffer_size = escaped_size - (pos - escaped);
		switch (*html_content) {
		case '"':
			pos += snprintf(pos, buffer_size, "%s", "&quot;");
			break;
		case '\'':
			pos += snprintf(pos, buffer_size, "%s", "&apos;");
			break;
		case '&':
			pos += snprintf(pos, buffer_size, "%s", "&amp;");
			break;
		case '<':
			pos += snprintf(pos, buffer_size, "%s", "&lt;");
			break;
		case '>':
			pos += snprintf(pos, buffer_size, "%s", "&gt;");
			break;
		default:
			pos += snprintf(pos, buffer_size, "%c", *html_content);
		}
		html_content++;
	}

	return escaped;
}
