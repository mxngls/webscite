#include <errno.h>
#include <string.h>

#include "error.h"
#include "feed.h"
#include "ghist.h"
#include "html.h"

// custom web components and their standard HTML replacements for feed output
static const char* feed_tag_map[][2] = {
	{ "site-footnote", "aside" },
};
static const int feed_tag_map_len = sizeof(feed_tag_map) / sizeof(feed_tag_map[0]);

int create_feed(char* output_path, page_entry_arr* entry_arr)
{

	int res = 0;

	FILE* dest_file = NULL;
	if ((dest_file = fopen(output_path, "w")) == NULL) {
		ERRORF(SITE_ERROR_FILE_CREATE, output_path)
		return -1;
	}

	char feed_uri[] = _SITE_EXT_URL "/feed.atom";

	// make feed updated date RFC-3339 compliant
	size_t feed_modified_size = 256;
	char feed_modified[feed_modified_size];
	ghist_format_ts(
	    "%Y-%m-%dT00:00:00Z", feed_modified,
	    entry_arr->elems[entry_arr->len - 1]->meta.modified);

	res = fprintf(
	    dest_file,
	    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	    "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
	    "    <title>%s</title>\n"
	    "    <link href=\"%s\" rel=\"alternate\"/>\n"
	    "    <link href=\"%s\" rel=\"self\"/>\n"
	    "    <updated>%s</updated>\n"
	    "    <author>\n"
	    "        <name>%s</name>\n"
	    "    </author>\n",
	    _SITE_EXT_TITLE, _SITE_EXT_URL, feed_uri, feed_modified, _SITE_EXT_AUTHOR);

	// use date-only format for TAG URI
	res = fprintf(
	    dest_file, "    <id>tag:www.%s,%s:%s</id>\n", _SITE_EXT_HOST, _SITE_EXT_TAG_SCHEME_DATE,
	    _SITE_EXT_FEED_ID);

	for (int i = 0; i < entry_arr->len; i++) {
		page_entry entry = *entry_arr->elems[i];

		// pages that opted out of being a post stay out of the feed
		if (!entry.headers.is_post) {
			continue;
		}

		size_t created_formatted_size = 256;
		char created_formatted[created_formatted_size];
		ghist_format_ts("%Y-%m-%dT00:00:00Z", created_formatted, entry.meta.created);

		char modified_formatted[256];
		if (entry.meta.modified) {
			ghist_format_ts(
			    "%Y-%m-%dT00:00:00Z", modified_formatted, entry.meta.modified);
		}

		// replace custom elements with standard HTML for feed readers
		char* feed_content = strdup(entry_arr->elems[i]->content);
		if (feed_content == NULL) {
			ERRORF(SITE_ERROR_MEMORY_ALLOCATION, entry.meta.path);
			goto error;
		}
		for (int t = 0; t < feed_tag_map_len; t++) {
			const char* custom = feed_tag_map[t][0];
			const char* standard = feed_tag_map[t][1];

			char open_tag[64], open_repl[64];
			snprintf(open_tag, sizeof(open_tag), "<%s", custom);
			snprintf(open_repl, sizeof(open_repl), "<%s", standard);
			size_t open_tag_len = strlen(open_tag);
			size_t open_repl_len = strlen(open_repl);

			char close_tag[64], close_repl[64];
			snprintf(close_tag, sizeof(close_tag), "</%s", custom);
			snprintf(close_repl, sizeof(close_repl), "</%s", standard);
			size_t close_tag_len = strlen(close_tag);
			size_t close_repl_len = strlen(close_repl);

			// replace closing tags first to avoid partial match
			// e.g. `</site-footnote` matching opening `<site-footnote`
			char* pos = feed_content;
			while ((pos = strstr(pos, close_tag)) != NULL) {
				memcpy(pos, close_repl, close_repl_len);
				memmove(
				    pos + close_repl_len, pos + close_tag_len,
				    strlen(pos + close_tag_len) + 1);
				pos += close_repl_len;
			}

			pos = feed_content;
			while ((pos = strstr(pos, open_tag)) != NULL) {
				memcpy(pos, open_repl, open_repl_len);
				memmove(
				    pos + open_repl_len, pos + open_tag_len,
				    strlen(pos + open_tag_len) + 1);
				pos += open_repl_len;
			}
		}

		char* escaped_content = NULL;
		if (html_escape_content(feed_content, &escaped_content)) {
			goto error;
		};
		free(feed_content);
		if (escaped_content == NULL) {
			ERRORF(SITE_ERROR_MEMORY_ALLOCATION, entry.meta.path);
			goto error;
		}

		res = fprintf(
		    dest_file,
		    "    <entry>\n"
		    "        <title>%s</title>\n"
		    "        <content type=\"html\">\n"
		    "%s"
		    "        </content>\n"
		    "        <link href=\"" _SITE_EXT_URL "%s\"/>\n"
		    "        <id>tag:www.%s,%s:%s</id>\n"
		    "        <published>%s</published>\n"
		    "        <updated>%s</updated>\n"
		    "    </entry>\n",
		    entry.headers.title, escaped_content, entry.meta.path, _SITE_EXT_HOST,
		    _SITE_EXT_TAG_SCHEME_DATE, entry.meta.path, created_formatted,
		    modified_formatted);

		free(escaped_content);
	}

	res = fprintf(dest_file, "</feed>\n");
	goto cleanup;

error:
	res = -1;

cleanup:
	fclose(dest_file);

	return res;
}
