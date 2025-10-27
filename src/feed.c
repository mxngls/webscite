#include <errno.h>
#include <string.h>

#include "error.h"
#include "feed.h"
#include "ghist.h"
#include "html.h"

int create_feed(char *output_path, page_header_arr *header_arr) {

        int res = 0;

        FILE *dest_file = NULL;
        if ((dest_file = fopen(output_path, "w")) == NULL) {
                ERRORF(SITE_ERROR_FILE_CREATE, output_path)
                return -1;
        }

        char feed_uri[] = _SITE_URL "/feed.atom";

        // make feed updated date RFC-3339 compliant
        size_t feed_modified_size = 256;
        char feed_modified[feed_modified_size];
        ghist_format_ts("%Y-%m-%dT00:00:00Z", feed_modified,
                        header_arr->elems[header_arr->len - 1]->meta.modified);

        res = fprintf(dest_file,
                      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                      "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
                      "    <title>%s</title>\n"
                      "    <link href=\"%s\" rel=\"alternate\"/>\n"
                      "    <link href=\"%s\" rel=\"self\"/>\n"
                      "    <updated>%s</updated>\n"
                      "    <author>\n"
                      "        <name>%s</name>\n"
                      "    </author>\n",
                      _SITE_TITLE, _SITE_URL, feed_uri, feed_modified, _SITE_AUTHOR);

        // use date-only format for TAG URI
        res = fprintf(dest_file, "    <id>tag:www.%s,%s:%s</id>\n", _SITE_HOST,
                      _SITE_TAG_SCHEME_DATE, _SITE_FEED_ID);

        for (int i = 0; i < header_arr->len; i++) {
                // header_arr and content_arr grow together so no additonal checks necessary here
                page_header header = *header_arr->elems[i];

                size_t created_formatted_size = 256;
                char created_formatted[created_formatted_size];
                ghist_format_ts("%Y-%m-%dT00:00:00Z", created_formatted, header.meta.created);

                char modified_formatted[256];
                if (header.meta.modified) {
                        ghist_format_ts("%Y-%m-%dT00:00:00Z", modified_formatted,
                                        header.meta.modified);
                }

                char *escaped_content = html_escape_content(content_arr.elems[i]->content);
                if (escaped_content == NULL) {
                        ERRORF(SITE_ERROR_MEMORY_ALLOCATION, header.meta.path)
                        return -1;
                }

                res = fprintf(dest_file,
                              "    <entry>\n"
                              "        <title>%s</title>\n"
                              "        <content type=\"html\">\n"
                              "%s"
                              "        </content>\n"
                              "        <link href=\"%s\"/>\n"
                              "        <id>tag:www.%s,%s:%s</id>\n"
                              "        <published>%s</published>\n"
                              "        <updated>%s</updated>\n"
                              "    </entry>\n",
                              header.title, escaped_content, header.meta.path, _SITE_HOST,
                              _SITE_TAG_SCHEME_DATE, header.meta.path, created_formatted,
                              modified_formatted);

                free(escaped_content);
        }

        res = fprintf(dest_file, "</feed>\n");

        return res;
}
