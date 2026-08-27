#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "page.h"

#include <stddef.h>

static const struct {
	const char* key;
	size_t offset; /* offset of a bool inside page_entry */
} includes[] = {
	{ "include_header", offsetof(page_entry, includes.header) },
	{ "include_footer", offsetof(page_entry, includes.footer) },
	{ "include_title",  offsetof(page_entry, includes.title)	},
	{ "include_date",	  offsetof(page_entry, includes.date)   },
};

static int parse_header_val(page_entry* entry, const char* key, char* value)
{
	bool* field = NULL;
	bool val = false;

	for (char* p = value; *p; p++)
		*p = (char)tolower((unsigned char)*p);

	if (strcmp(key, "is_post") == 0) {
		if (strcmp(value, "n") == 0 || strcmp(value, "no") == 0) {
			entry->is_post = false;
			entry->includes.title = false;
			entry->includes.date = false;
			entry->includes.header = true;
			entry->includes.footer = true;
			return 0;
		}
		if (strcmp(value, "y") == 0 || strcmp(value, "yes") == 0) {
			return 0;
		}
		ERRORF(SITE_ERROR_WRONG_HEADER_VAL, entry->meta.source_path, value);
		return -1;
	}

	for (size_t i = 0; i < sizeof includes / sizeof *includes; i++) {
		if (strcmp(key, includes[i].key) == 0) {
			field = (bool*)((char*)entry + includes[i].offset);
			break;
		}
	}

	if (!field) {
		ERRORF(SITE_ERROR_WRONG_HEADER_KEY, entry->meta.source_path, key)
		return -1;
	}

	if (strcmp(value, "n") == 0 || strcmp(value, "no") == 0)
		val = false;
	else if (strcmp(value, "y") == 0 || strcmp(value, "yes") == 0) {
		val = true;
	} else {
		ERRORF(SITE_ERROR_WRONG_HEADER_VAL, entry->meta.source_path, value);
		return -1;
	}

	if (field) {
		*field = val;
	}
	return 0;
}

int page_parse_header(FILE* file, page_entry* entry)
{
	int ret = 0;
	char* line = NULL;
	size_t len = 0;
	ssize_t read = 0;
	ssize_t readt = 0;

	entry->title = NULL;

	while ((read = getline(&line, &len, file)) != -1) {
		readt += read;

		// a blank line terminates the header
		if (read == 1 || line[0] == '\n') {
			break;
		}

		// remove newline
		if (line[read - 1] == '\n') {
			line[read - 1] = '\0';
			read--;
		}

		// split fields
		char* colon = strchr(line, ':');
		if (!colon)
			continue;

		// key-value pair
		*colon = '\0';
		char* key = line;
		char* value = colon + 1;

		while (isspace(*value)) {
			value++;
		}

		if (!value) {
			value = NULL;
		}

		if (strncmp(key, "title", read) == 0) {
			entry->title = strdup(value);
		} // manual post override
		else if (parse_header_val(entry, key, value)) {
			goto error;
		}
	}

	// every page needs a title, posts and non-posts alike
	if (!entry->title) {
		ERRORF(SITE_ERROR_MISSING_HEADERS, entry->meta.source_path);
		goto error;
	}

	ret = (int)readt;
	goto cleanup;

error:
	ret = -1;

cleanup:
	free(line);

	return ret;
}

int page_parse_content(FILE* source_file, char* source_path, size_t content_size, char* content)
{
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
