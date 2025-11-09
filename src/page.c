#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "page.h"

int page_parse_header(FILE *file, page_header *header) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;
        ssize_t readt = 0;

        header->title = NULL;

        bool in_header = true;
        while (in_header && (readt += read = getline(&line, &len, file))) {
                // newline
                if (read <= 1 || line[0] == '\n') {
                        in_header = false;
                        break;
                }

                // remove newline
                if (line[read - 1] == '\n') {
                        line[read - 1] = '\0';
                        read--;
                }

                // split fields
                char *colon = strchr(line, ':');
                if (!colon) continue;

                // key-value pair
                *colon = '\0';
                char *key = line;
                char *value = colon + 1;

                while (isspace(*value)) {
                        value++;
                }

                if (!value) {
                        value = NULL;
                }

                if (strncmp(key, "title", read) == 0) header->title = strdup(value);
        }

        free(line);

        if (!header->title) return -1;

        return (int)readt;
}

int page_parse_content(FILE *source_file, char *source_path, size_t content_size, char *content) {
        int ret = 0;

        size_t bytes_read = fread(content, 1, content_size, source_file);
        if (bytes_read != content_size) {
                if (feof(source_file)) {
                        printf("Page has no content. Aborting.\n");
                } else if (ferror(source_file)) {
                        ERRORF(SITE_ERROR_FILE_READ, source_path);
                } else {
                        ERRORF(SITE_ERROR_UNEXPECTED_EOF, source_path);
                }
                ret = -1;
        }
        content[bytes_read] = '\0';

        return ret;
}
