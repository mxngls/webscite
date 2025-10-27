#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "page.h"

int page_parse_header(FILE *file, page_header *header) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;
        ssize_t readt = 0;

        header->title = NULL;
        header->subtitle = NULL;

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
                else if (strncmp(key, "subtitle", read) == 0) header->subtitle = strdup(value);
        }

        free(line);

        if (!header->title || !header->subtitle) return -1;

        return (int)readt;
}
