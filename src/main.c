#include <errno.h>
#include <fts.h>
#include <ftw.h>
#include <string.h>

#include "error.h"
#include "feed.h"
#include "ghist.h"
#include "html.h"
#include "page.h"

#ifndef _SITE_EXT_TARGET_DIR
#define _SITE_EXT_TARGET_DIR "docs"
#endif

#ifndef _SITE_EXT_GIT_DIR
#define _SITE_EXT_GIT_DIR ".git"
#endif

#define _SITE_INDEX_PATH "index.htm"
// UNUSED #define _SITE_ABOUT_PATH "about.htm"

#define _SITE_EXCEMPT_LIST ""

const char *index_excempt_arr[] = {_SITE_EXCEMPT_LIST};
#define _SITE_EXCEMPT_LIST_COUNT (sizeof(index_excempt_arr) / sizeof(index_excempt_arr[0]))

page_content_arr content_arr = {
    .elems = {0},
    .len = 0,
};

tracked_file_arr tracked_arr = {
    .files = NULL,
    .len = 0,
    .capacity = 0,
};

// utils
static int __copy_file(char *, char *);
static FTS *__init_fts(char *);
static int __create_dir(char *);

// main routines
static page_header *__process_page_file(FTSENT *);
static int __process_index_file(char *, page_header_arr *);

static int __copy_file(char *from, char *to) {
        FILE *from_file = NULL;
        FILE *to_file = NULL;

        if ((from_file = fopen(from, "r")) == NULL) {
                ERRORF(SITE_ERROR_FILE_OPEN_READ, from);
                return -1;
        }

        if ((to_file = fopen(to, "w")) == NULL) {
                ERRORF(SITE_ERROR_FILE_OPEN_WRITE, to);
                fclose(from_file);
                return -1;
        }

        char *line = NULL;
        size_t bufsize = 0;
        ssize_t len = 0;
        int res = 0;

        while ((len = getline(&line, &bufsize, from_file)) > 0) {
                if (fwrite(line, 1, (size_t)len, to_file) != (size_t)len) {
                        ERRORF(SITE_ERROR_FILE_WRITE, from);
                        res = -1;
                        break;
                }
        }

        if (len < 0 && !feof(from_file) && ferror(from_file)) {
                ERRORF(SITE_ERROR_UNEXPECTED_EOF, from);
                res = -1;
        }

        free(line);
        fclose(from_file);
        fclose(to_file);

        return res;
}

static int __create_dir(char *dir_name) {
        mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

        if (mkdir(dir_name, mode) != 0 && errno != EEXIST) {
                ERRORF(SITE_ERROR_DIRECTORY_CREATE, dir_name);
                return -1;
        }

        return 0;
}

static FTS *__init_fts(char *source) {
        FTS *ftsp = NULL;
        char *paths[] = {(char *)source, NULL};
        int _fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;

        if ((ftsp = fts_open(paths, _fts_options, NULL)) == NULL) {
                ERROR(SITE_ERROR_FTS_INIT);
                return NULL;
        }

        if (fts_children(ftsp, 0) == NULL) {
                printf("No pages to convert. Aborting\n");
                return NULL;
        }

        return ftsp;
}

static page_header *__process_page_file(FTSENT *ftsentp) {
        page_header *res = NULL;
        char *source_path = ftsentp->fts_path;
        FILE *source_file = NULL;
        tracked_file *tracked = NULL;
        page_header *header = NULL;
        char *page_content = NULL;

        if ((source_file = fopen(ftsentp->fts_path, "r")) == NULL) {
                ERRORF(SITE_ERROR_FILE_READ, source_path);
                goto error;
        }

        // convert extension to proper .html
        char page_name[256] = "\0";
        snprintf(page_name, sizeof(page_name), "%s", ftsentp->fts_name);
        strlcat(page_name, "l", sizeof(page_name));

        // output path
        char page_path[_SITE_PATH_MAX];
        snprintf(page_path, sizeof(page_path), "%s/%s", _SITE_EXT_TARGET_DIR, page_name);

        if ((header = calloc(1, sizeof(page_header))) == NULL) {
                ERROR(SITE_ERROR_MEMORY_ALLOCATION);
                goto error;
        }
        char page_href[100] = "/";
        strcat(page_href, page_name);
        strncpy(header->meta.path, page_href, _SITE_PATH_MAX - 1);

        if ((tracked = ghist_find_by_path(source_path))) {
                header->meta.created = tracked->creat_time;
                header->meta.modified = tracked->mod_time;
        }

        // read content
        int header_len = -1;
        if ((header_len = page_parse_header(source_file, header)) == -1) {
                ERRORF(SITE_ERROR_MISSING_HEADERS, source_path);
                goto error;
        };
        size_t content_size = ftsentp->fts_statp->st_size - header_len;
        page_content = malloc(content_size + 1);
        if (page_content == NULL) {
                ERROR(SITE_ERROR_MEMORY_ALLOCATION);
                goto error;
        }
        size_t bytes_read = fread(page_content, 1, content_size, source_file);
        if (bytes_read != content_size) {
                if (feof(source_file)) {
                        printf("Page has no content. Aborting.\n");
                } else if (ferror(source_file)) {
                        ERRORF(SITE_ERROR_FILE_READ, source_path);
                } else {
                        ERRORF(SITE_ERROR_UNEXPECTED_EOF, source_path);
                }
                goto error;
        }
        page_content[bytes_read] = '\0';

        // create valid html file
        if (html_create_page(header, page_content, page_path) != 0) {
                goto error;
        };

        res = header;
        header = NULL;
        goto cleanup;

error:
        res = NULL;

cleanup:
        if (source_file) fclose(source_file);
        if (page_content) free(page_content);
        if (header) free(header);

        return res;
}

static int __process_index_file(char *index_file_path, page_header_arr *header_arr) {
        int res = 0;
        FILE *source_file = NULL;
        char *page_content = NULL;

        if ((source_file = fopen(index_file_path, "r")) == NULL) {
                ERRORF(SITE_ERROR_FILE_OPEN_READ, index_file_path);
                goto error;
        }

        struct stat source_file_stat;
        if (stat(index_file_path, &source_file_stat) != 0) {
                ERRORF(SITE_ERROR_FILE_STAT, index_file_path);
                goto error;
        }

        // output path
        char page_path[_SITE_PATH_MAX];
        char *filename = strrchr(index_file_path, '/');
        filename ? filename++ : (filename = index_file_path);
        snprintf(page_path, sizeof(page_path), "%s/%s", _SITE_EXT_TARGET_DIR, filename);

        size_t content_size = source_file_stat.st_size;
        if ((page_content = malloc(content_size + 1)) == NULL) {
                ERROR(SITE_ERROR_MEMORY_ALLOCATION);
                goto error;
        }

        ssize_t bytes_read = fread(page_content, 1, source_file_stat.st_size, source_file);
        if (bytes_read != source_file_stat.st_size) {
                if (feof(source_file)) {
                        printf("Page has no content. Aborting.\n");
                } else if (ferror(source_file)) {
                        ERRORF(SITE_ERROR_FILE_READ, index_file_path);
                } else {
                        ERRORF(SITE_ERROR_UNEXPECTED_EOF, index_file_path);
                }
                goto error;
        }
        page_content[bytes_read] = '\0';
        res = html_create_index(page_content, page_path, header_arr, index_excempt_arr,
                                _SITE_EXCEMPT_LIST_COUNT);
        goto cleanup;

error:
        res = -1;

cleanup:
        if (page_content) free(page_content);
        if (source_file) fclose(source_file);

        return res;
}

int main(void) {
        int res = 0;
        FTS *ftsp = NULL;
        FTSENT *ftsentp = NULL;

        page_header_arr header_arr = {
            .elems = {0},
            .len = 0,
        };

        if (__create_dir(_SITE_EXT_TARGET_DIR) != 0) {
                res = -1;
                return res;
        }

        if (html_init_templates() != 0) {
                res = -1;
                return res;
        }

        if (ghist_times()) {
                res = -1;
                goto cleanup;
        }

        if ((ftsp = __init_fts(_SITE_SOURCE_DIR)) == NULL) {
                res = -1;
                goto cleanup;
        }

        while ((ftsentp = fts_read(ftsp)) != NULL) {
                // only process files at the top level
                if (ftsentp->fts_level > 1) continue;

                // we only care for plain non-hidden __files__
                if (ftsentp->fts_info != FTS_F) continue;
                if (ftsentp->fts_name[0] == '.') continue;

                char *dot = strrchr(ftsentp->fts_name, '.');
                if (dot == NULL) continue;
                char *ext = dot + 1;

                // non-html files
                if (strcmp(ext, "htm") != 0) {
                        char to_path[_SITE_PATH_MAX];
                        to_path[0] = '\0';

                        strlcat(to_path, _SITE_EXT_TARGET_DIR, sizeof(to_path));
                        size_t path_len = strlen(to_path);

                        // possibly add path separator
                        if (path_len > 0 && to_path[path_len - 1] != '/' &&
                            path_len + 1 < sizeof(to_path)) {
                                to_path[path_len] = '/';
                                to_path[path_len + 1] = '\0';
                        }

                        strlcat(to_path, ftsentp->fts_name, sizeof(to_path));

                        __copy_file(ftsentp->fts_path, to_path);
                        continue;
                }

                // ignore index for now
                if (strcmp(ftsentp->fts_name, _SITE_INDEX_PATH) == 0) {
                        continue;
                }

                page_header *header = NULL;
                if ((header = __process_page_file(ftsentp)) == NULL) {
                        res = -1;
                } else {
                        header_arr.elems[header_arr.len] = header;
                        header_arr.len++;
                }
        }

        if (__process_index_file(_SITE_SOURCE_DIR "/" _SITE_INDEX_PATH, &header_arr) != 0) {
                res = -1;
        }

        if (create_feed(_SITE_EXT_TARGET_DIR "feed.atom", &header_arr) == 0) {
                res = -1;
        }

cleanup:
        // cleanup
        if (ftsp) fts_close(ftsp);
        // headers
        for (int i = 0; i < header_arr.len; i++) {
                free((char *)header_arr.elems[i]->title);
                free((char *)header_arr.elems[i]->subtitle);
                free(header_arr.elems[i]);
        }
        // tracked files (renamed files are to be cleaned
        for (int i = 0; i < tracked_arr.len; i++) {
                free(tracked_arr.files[i].file_path);
        }
        free(tracked_arr.files);

        html_cleanup_templates();

        return res;
}
