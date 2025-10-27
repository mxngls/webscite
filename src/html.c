#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "error.h"
#include "ghist.h"
#include "html.h"
#include "page.h"

// global template content
char *site_menu = NULL;

// compare by creation time
static int __qsort_cb(const void *a, const void *b) {
        page_header *header_a = *(page_header **)a;
        page_header *header_b = *(page_header **)b;

        // descending order (newest first)
        if (header_a->meta.created > header_b->meta.created) return -1;
        if (header_a->meta.created < header_b->meta.created) return 1;
        return 0;
}

// shared template building blocks
static int __html_parse_block(const char *block_path, page_block *block) {
        FILE *block_file = NULL;
        char *block_content = NULL;
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
        if (block_file) fclose(block_file);
        if (block_content) free(block_content);

        return res;
}

// initialize all templates
int html_init_templates(void) {
        page_block menu_block = {0};

        // load menu
        if (__html_parse_block(_SITE_BLOCK_DIR_PATH "/menu.htm", &menu_block) != 0) {
                goto error;
        }

        // transfer ownership
        site_menu = menu_block.content;

        return 0;

error:
        if (menu_block.content) free(menu_block.content);
        return -1;
}

// cleanup templates
void html_cleanup_templates(void) {
        if (site_menu) {
                free(site_menu);
                site_menu = NULL;
        }
}

// package content
static char *__html_create_content(page_header *header, char *page_content) {

        size_t buf_size = 24 * 1024;
        char *buf = NULL;
        if ((buf = malloc(buf_size)) == NULL) {
                ERROR(SITE_ERROR_MEMORY_ALLOCATION)
                return NULL;
        }

        char *pos = buf;
        int offset = 0;

        size_t created_formatted_size = 256;
        char created_formatted[created_formatted_size];
        if (header->meta.created) {
                ghist_format_ts("%Y-%m-%d", created_formatted, header->meta.created);
        } else {
                snprintf(created_formatted, sizeof(created_formatted), "%s", "DRAFT");
        }

        // check for errors
        if (offset < 0) {
                ERROR(SITE_ERROR_FILE_WRITE);
                free(buf);
                return NULL;
        }

        pos += offset;

        // separate main content from header group
        offset = snprintf(pos, buf_size - offset, "%s\n", "<div id=\"post-body\">");
        pos += offset;

        // add header
        char *upper = malloc(strlen(header->title) + 1);
        if (upper == NULL) {
                ERROR(SITE_ERROR_MEMORY_ALLOCATION);
                free(buf);
                return NULL;
        }
        strcpy(upper, header->title);
        char *p = upper;
        while (*p) {
                *p = (char)toupper((unsigned char)*p);
                p++;
        }
        offset = snprintf(pos, buf_size - offset, "<h1>%s</h1>\n", upper);
        free(upper);
        pos += offset;

        // add content
        char *line = strtok(page_content, "\n");
        while (line) {
                if (*line) {
                        offset = snprintf(pos, buf_size, "%s\n", line);
                        pos += offset;
                }
                line = strtok(NULL, "\n");
        }

        // close main content
        offset = snprintf(pos, buf_size - offset, "%s\n", "</div>");
        pos += offset;

        // add updated date at the end if present
        int has_modified = header->meta.modified != 0;
        if (has_modified) {
                char modified_formatted[256];
                ghist_format_ts("%Y-%m-%d", modified_formatted, header->meta.modified);
                offset = snprintf(pos, buf_size - offset,
                                  // clang-format off
                                  "<div id=\"post-date\">\n"
                                      "<div id=\"date-created\">\n"
                                          "<small>Created on %s</small>\n"
                                      "</div>\n"
				      "|\n"
                                      "<div id=\"date-updated\">\n"
                                          "<small>Last Updated on %s</small>\n"
                                      "</div>\n"
                                  "</div>\n",
                                  // clang-format on
                                  created_formatted, modified_formatted);
                pos += offset;
        } else {
                offset = snprintf(pos, buf_size - offset,
                                  // clang-format off
                                  "<div id=\"post-date\">\n"
                                      "<div id=\"date-created\">\n"
                                          "<small>Created on %s</small>\n"
                                      "</div>\n"
                                  "</div>\n",
                                  // clang-format on
                                  created_formatted);
                pos += offset;
        }

        return buf;
}

// create plain html file
int html_create_page(page_header *header, char *plain_content, char *output_path) {
        // html destination
        FILE *dest_file = fopen(output_path, "w");
        if (dest_file == NULL) {
                ERRORF(SITE_ERROR_FILE_CREATE, output_path);
                free(header);
                return -1;
        }

        int fprintf_ret = 0;

        fprintf_ret = fprintf(
            dest_file,
            // clang-format off
            "<!DOCTYPE html>"
            "<html lang=\"en\">\n"
            "<head>\n"
            "    <meta charset=\"utf-8\">\n"
            "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "    <meta name=\"apple-mobile-web-app-capable\" content=\"yes\">\n"
            "    <meta name=\"apple-mobile-web-app-status-bar-style\" content=\"default\">\n"
            "    <meta name=\"theme-color\" content=\"var(--color-bg)\" media=\"(prefers-color-scheme: light)\">\n"
            "    <meta name=\"theme-color\" content=\"var(--color-bg)\" media=\"(prefers-color-scheme: dark)\">\n"
            "	 <link href=\"/feed.atom\" type=\"application/atom+xml\" rel=\"alternate\">\n"
            "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\">\n"
            "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\">\n" _SITE_HTML_FONT "\n"
            "    <title>%s</title>\n"
	    "	 <style>@import url('https://fonts.googleapis.com/css2?family=Rock+3D&display=swap');</style>\n"
            "    %s\n"
            "</head>\n"
            "<body>\n"
	    "    <div id=\"background\"></div>\n"
	    "        <div id=\"post\" class=\"content\">\n"
	    "            %s\n"
            "            <main>\n"
	    "                <article id=\"post-main\">\n",
            // clang-format on
            _SITE_STYLE_SHEET_PATH, _SITE_MENU_STYLE_SHEET_PATH, header->title, _SITE_SCRIPT,
            site_menu);

        // write content
        char *html_content = NULL;
        if ((html_content = __html_create_content(header, plain_content)) == NULL) {
                fclose(dest_file);
                return -1;
        }

        page_content *page_content = NULL;
        if ((page_content = malloc(sizeof(*page_content))) == NULL) {
                free(html_content);
                fclose(dest_file);
                return -1;
        }
        page_content->content = html_content;
        strcpy(page_content->meta.path, header->meta.path);

        content_arr.elems[content_arr.len] = page_content;
        content_arr.len++;
        fprintf_ret = fprintf(dest_file, "%s", html_content);

        // close html
        // clang-format off
        fprintf_ret = fprintf(dest_file, "            </article>\n"
                                         "        </main>\n"
                                         "    </div>\n"
                                         "</body>\n"
                                         "</html>\n");
        // clang-format on

        if (fprintf_ret < 0) {
                ERRORF(SITE_ERROR_FILE_WRITE, dest_file);
                fclose(dest_file);
                return -1;
        }

        fclose(dest_file);

        return 0;
}

// create html index file
int html_create_index(char *page_content, char *output_path, page_header_arr *header_arr,
                      const char *index_excempt_arr[], int index_excempt_arr_n) {
        // html destination
        FILE *dest_file = fopen(output_path, "w");
        if (dest_file == NULL) {
                ERRORF(SITE_ERROR_FILE_CREATE, output_path);
                return -1;
        }

        int fprintf_ret = 0;

        fprintf_ret = fprintf(
            dest_file,
            // clang-format off
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "    <head>\n"
            "    <meta charset=\"utf-8\">\n"
            "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "    <meta name=\"apple-mobile-web-app-capable\" content=\"yes\">\n"
            "    <meta name=\"apple-mobile-web-app-status-bar-style\" content=\"default\">\n"
            "    <meta name=\"theme-color\" content=\"var(--color-bg)\" media=\"(prefers-color-scheme: light)\">\n"
            "    <meta name=\"theme-color\" content=\"var(--color-bg)\" media=\"(prefers-color-scheme: dark)\">\n"
            "    <link href=\"/feed.atom\" type=\"application/atom+xml\" rel=\"alternate\">\n"
            "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\">\n"
            "    <link rel=\"stylesheet\" href=\"%s\" type=\"text/css\">\n" _SITE_HTML_FONT "\n"
            "    <title>%s</title>\n"
	    "	 <style>@import url('https://fonts.googleapis.com/css2?family=Rock+3D&display=swap');</style>\n"
            "	 %s\n"
            "</head>\n"
            "<body>\n"
	    "    <div id=\"background\"></div>\n"
	    "        <div id=\"index\" class=\"content\">\n"
	    "            %s\n"
            "            <main>\n",
            // clang-format on
            _SITE_STYLE_SHEET_PATH, _SITE_MENU_STYLE_SHEET_PATH, _SITE_TITLE, _SITE_SCRIPT,
            site_menu);

        // content
        char *dest_line = strtok((char *)page_content, "\n");
        while (dest_line) {
                if (!*dest_line) continue;
                fprintf_ret = fprintf(dest_file, "%s\n", dest_line);
                dest_line = strtok(NULL, "\n");
        }

        // sort by creation time
        qsort(header_arr->elems, header_arr->len, sizeof(page_header *), __qsort_cb);

        // add a list of posts to the index
        fprintf_ret = fprintf(dest_file, "<section id=\"post-list\">\n"
                                         "    <ul>\n");

        for (int i = 0; i < header_arr->len; i++) {
                bool skip = false;
                for (int j = 0; j < index_excempt_arr_n; j++) {
                        int path_len = (int)strlen(header_arr->elems[i]->meta.path);
                        if (path_len > 1 && strncmp(header_arr->elems[i]->meta.path + 1,
                                                    index_excempt_arr[j], path_len - 2) == 0)
                                skip = true;
                }
                if (skip) continue;
                size_t created_formatted_size = 256;
                char created_formatted[created_formatted_size];
                if (header_arr->elems[i]->meta.created) {
                        ghist_format_ts("%Y", created_formatted,
                                        header_arr->elems[i]->meta.created);
                } else {
                        snprintf(created_formatted, sizeof(created_formatted), "%s", "DRAFT");
                }

                fprintf_ret = fprintf(dest_file,
                                      // clang-format off
				      "<li>\n"
                    		          "<span class=\"date\">%s</span>\n"
                    		          "<a href=\"%s\">\n"
					      "<span class=\"title\">%s</span>\n"
                    		          "</a>\n"
                    		      "</li>\n",
                                      // clang-format on
                                      created_formatted, header_arr->elems[i]->meta.path,
                                      header_arr->elems[i]->title);
        }

        fprintf_ret = fprintf(dest_file, "    </ul>\n"
                                         "</section>\n");

        // close <main>
        // clang-format off
        fprintf_ret = fprintf(dest_file, "        </main>\n"
					 "    </div>\n"
                                         "</body>\n"
                                         "</html>\n");
        // clang-format on

        if (fprintf_ret < 0) {
                ERRORF(SITE_ERROR_FILE_WRITE, dest_file);
                fclose(dest_file);
                return -1;
        }

        fclose(dest_file);

        return 0;
}

// escape html entities
char *html_escape_content(char *html_content) {
        int content_size = 0;
        char *html_content_copy = html_content;
        while (*html_content_copy++) {
                content_size++;
        }

        // quiete conservative estimate
        unsigned long escaped_size = (unsigned long)(content_size * 2);
        char *escaped = malloc(escaped_size);
        escaped[0] = '\0';

        while (*html_content) {
                switch (*html_content) {
                case '"':
                        strcat(escaped, "&quot;");
                        break;
                case '\'':
                        strcat(escaped, "&#39;");
                        break;
                case '&':
                        strcat(escaped, "&amp;");
                        break;
                case '<':
                        strcat(escaped, "&lt;");
                        break;
                case '>':
                        strcat(escaped, "&gt;");
                        break;
                default:
                        strncat(escaped, html_content, 1);
                }
                html_content++;
        }
        return escaped;
}
