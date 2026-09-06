#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "page.h"

#include <stddef.h>

static int parse_str_val(char** dst, const char* val)
{
	char* dup = strdup(val);
	if (!dup) {
		if (errno == ENOMEM) {
			ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		}
		return -1;
	}

	free(*dst);
	*dst = dup;

	return 0;
}

static int parse_bool_val(const char* v, bool* out)
{
	if (strcasecmp(v, "y") == 0 || strcasecmp(v, "yes") == 0) {
		*out = true;
		return 0;
	}
	if (strcasecmp(v, "n") == 0 || strcasecmp(v, "no") == 0) {
		*out = false;
		return 0;
	}
	return -1;
}

static int header_set(page_entry* entry, const char* key, const char* value)
{
	// string headers
	if (strcmp(key, "title") == 0) {
		return parse_str_val(&entry->headers.title, value);
	} else if (strcmp(key, "class") == 0) {
		return parse_str_val(&entry->headers.class, value);
	}

	bool* field = NULL;
	if (strcmp(key, "is_post") == 0) {
		field = &entry->headers.is_post;
	} else if (strcmp(key, "include_header") == 0) {
		field = &entry->headers.include_header;
	} else if (strcmp(key, "include_footer") == 0) {
		field = &entry->headers.include_footer;
	} else if (strcmp(key, "include_title") == 0) {
		field = &entry->headers.include_title;
	} else if (strcmp(key, "include_date") == 0) {
		field = &entry->headers.include_date;
	} else {
		ERRORF(SITE_ERROR_WRONG_HEADER_KEY, entry->meta.source_path, field);
		return -1;
	}

	// boolean headers
	if (parse_bool_val(value, field)) {
		ERRORF(
		    SITE_ERROR_WRONG_HEADER_VAL, entry->meta.source_path, value,
		    "Allowed are: \"yes\"/\"y\" or \"no\"/\"n\"");
		return -1;
	}

	return 0;
}

static int parse_header(page_entry* entry, const char* key, char* value)
{
	if (strcmp(key, "is_post") == 0) {
		if (strcmp(value, "n") == 0 || strcmp(value, "no") == 0) {
			entry->headers.is_post = false;
			entry->headers.include_title = false;
			entry->headers.include_date = false;
			entry->headers.include_header = true;
			entry->headers.include_footer = true;
			return 0;
		}
		if (strcmp(value, "y") == 0 || strcmp(value, "yes") == 0) {
			return 0;
		}
		ERRORF(SITE_ERROR_WRONG_HEADER_VAL, entry->meta.source_path, value);
		return -1;
	}

	if (header_set(entry, key, value)) {
		return -1;
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

	entry->headers.title = NULL;

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

		if (parse_header(entry, key, value)) {
			goto error;
		}
	}

	// every page needs a title, posts and non-posts alike
	if (!entry->headers.title) {
		ERRORF(SITE_ERROR_MISSING_HEADERS, "title", entry->meta.source_path);
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
