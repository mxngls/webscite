#include <errno.h>
#include <fts.h>
#include <ftw.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "error.h"
#include "feed.h"
#include "ghist.h"
#include "html.h"
#include "page.h"

const char* index_excempt_arr[] = { _SITE_EXEMPT_LIST };
#define _SITE_EXEMPT_LIST_COUNT (int)((sizeof(index_excempt_arr) / sizeof(index_excempt_arr[0])))

page_content_arr content_arr = {
	.elems = { 0 },
	.len = 0,
};

tracked_file_arr tracked_arr = {
	.files = NULL,
	.len = 0,
	.capacity = 0,
};

typedef struct {
	char* name;
	char* path;
	off_t size;
} page_info;

// utils
static int __copy_file(char*, char*);
static FTS* __init_fts(char*);
static int __create_dir(char*);
static int __validate_ext_dirs(char*, char*);
static char* __extract_ext_prefix(char*);
static char* __extract_dir(char*, bool);

// main routine
static page_result* __process_page_file(
    page_info*,
    char*,
    page_header_arr*,
    bool,
    bool,
    bool,
    bool);

static int __copy_file(char* from, char* to)
{
	FILE* from_file = NULL;
	FILE* to_file = NULL;

	if ((from_file = fopen(from, "r")) == NULL) {
		ERRORF(SITE_ERROR_FILE_OPEN_READ, from);
		return -1;
	}

	if ((to_file = fopen(to, "w")) == NULL) {
		ERRORF(SITE_ERROR_FILE_OPEN_WRITE, to);
		fclose(from_file);
		return -1;
	}

	char* line = NULL;
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

static int __create_dir(char* dir_name)
{
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

	if (mkdir(dir_name, mode) != 0 && errno != EEXIST) {
		ERRORF(SITE_ERROR_DIRECTORY_CREATE, dir_name);
		return -1;
	}

	return 0;
}

// verify that _SITE_EXT_SOURCE_DIR belongs to the _SITE_EXT_GIT_DIR
static int __validate_ext_dirs(char* git_path, char* source_path)
{
	int res = 0;

	char* git_path_copy = NULL;

	// obtain absolute paths from given path parameters
	char* git_absolute_path = realpath(git_path, NULL);
	char* source_absolute_path = realpath(source_path, NULL);
	char* source_start = source_absolute_path;
	if (!git_absolute_path || !source_absolute_path) {
		ERROR(SITE_ERROR_EXT_DIRS_INVALID);
		res = -1;
		goto cleanup;
	}

	if ((git_path_copy = malloc(_SITE_PATH_MAX - 1)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		res = -1;
		goto cleanup;
	};
	strncpy(git_path_copy, git_absolute_path, _SITE_PATH_MAX - 1);

	// compare overlapping part of paths
	char* git_root = dirname(git_path_copy);
	do {
		if (*git_root != *source_start) {
			ERROR(SITE_ERROR_EXT_DIRS_NONMATCHING);
			res = -1;
			goto cleanup;
		}

		++git_root;
		++source_start;

	} while (*git_root);

	// check if provided source points to a directory contained inside the repository root
	if (*git_root == '\0' && *source_start != '/') {
		ERROR(SITE_ERROR_EXT_DIRS_NONMATCHING);
		res = -1;
		goto cleanup;
	}

cleanup:
	if (git_absolute_path)
		free(git_absolute_path);
	if (source_absolute_path)
		free(source_absolute_path);
	if (git_path_copy)
		free(git_path_copy);

	return res;
}

static char* __extract_ext_prefix(char* ext_path)
{
	char* prefix = NULL;

	char* ext_path_copy = strdup(ext_path);

	// strip trailing slashes
	size_t len = strlen(ext_path_copy);
	while (len > 0 && ext_path_copy[len - 1] == '/') {
		ext_path_copy[len - 1] = '\0';
		len--;
	}

	char* last_slash = strrchr(ext_path_copy, '/');

	if (last_slash && last_slash != ext_path_copy) {
		size_t prefix_len = (size_t)(last_slash - ext_path_copy) + 1;
		char* path_prefix = NULL;
		if ((path_prefix = malloc(prefix_len + 1)) == NULL) {
			ERROR(SITE_ERROR_MEMORY_ALLOCATION);
			prefix = NULL;
			return prefix;
		};
		strncpy(path_prefix, ext_path_copy, prefix_len);
		path_prefix[prefix_len] = '\0';
		prefix = path_prefix;
	} else {
		prefix = strdup("");
	}

	free(ext_path_copy);

	return prefix;
}

static char* __extract_dir(char* path, bool is_dir)
{
	char* dir = NULL;
	char* parent_dir;
	char* source_dir;

	char* path_copy = strdup(path);

	if (is_dir) {
		source_dir = strstr(path_copy, _SITE_EXT_SOURCE_DIR);
	} else {
		parent_dir = dirname(path_copy);
		source_dir = strstr(parent_dir, _SITE_EXT_SOURCE_DIR);
	}

	if (!source_dir) {
		free(path_copy);
		dir = strdup("");
		return dir;
	}

	char* curr_dir = source_dir + strlen(_SITE_EXT_SOURCE_DIR);

	// skip leading slash
	if (curr_dir[0] == '/') {
		curr_dir++;
	}

	dir = strdup(curr_dir);

	free(path_copy);

	return dir;
}

static FTS* __init_fts(char* source)
{
	FTS* ftsp = NULL;
	char* paths[] = { (char*)source, NULL };
	int _fts_options = FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR;

	if ((ftsp = fts_open(paths, _fts_options, NULL)) == NULL) {
		ERROR(SITE_ERROR_FTS_INIT);
		return NULL;
	}

	if (fts_children(ftsp, 0) == NULL) {
		printf("No pages to convert. Aborting\n");
		fts_close(ftsp);
		return NULL;
	}

	return ftsp;
}

static page_result* __process_page_file(
    page_info* page_file,
    char* curr_dir,
    page_header_arr* header_arr,
    bool is_blog,
    bool include_back_ref,
    bool include_title,
    bool include_date)
{
	page_result* res = NULL;

	if ((res = malloc(sizeof(page_result))) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		return NULL;
	}
	res->header = NULL;
	res->content = NULL;

	char* source_path = page_file->path;
	FILE* source_file = NULL;

	tracked_file* tracked = NULL;

	page_header* header = NULL;
	char* content = NULL;

	if ((source_file = fopen(page_file->path, "r")) == NULL) {
		ERRORF(SITE_ERROR_FILE_READ, source_path);
		goto error;
	}

	// convert extension to proper .html
	char page_name[256] = "\0";
	snprintf(page_name, sizeof(page_name), "%s", page_file->name);
	strlcat(page_name, "l", sizeof(page_name));

	// output path
	char page_path[_SITE_PATH_MAX];
	if (curr_dir[0] == '\0') {
		snprintf(page_path, sizeof(page_path), "%s/%s", _SITE_EXT_TARGET_DIR, page_name);
	} else {
		snprintf(
		    page_path, sizeof(page_path), "%s/%s/%s", _SITE_EXT_TARGET_DIR, curr_dir,
		    page_name);
	}

	// handle page headers
	if ((header = calloc(1, sizeof(page_header))) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		goto error;
	}
	// populate page metadata
	char page_href[100];
	if (curr_dir[0] == '\0') {
		snprintf(page_href, sizeof(page_href), "/%s", page_name);
	} else {
		snprintf(page_href, sizeof(page_href), "%s/%s", curr_dir, page_name);
	}
	strncpy(header->meta.path, page_href, _SITE_PATH_MAX - 1);
	if ((tracked = ghist_find_by_path(source_path))) {
		header->meta.created = tracked->creat_time;
		header->meta.modified = tracked->mod_time;

		if (ghist_blob_hash(
			header->meta.hash, sizeof(header->meta.hash), tracked->file_path)) {
			goto error;
		}
	}
	// parse page header
	int header_len = -1;
	if ((header_len = page_parse_header(source_file, header)) == -1) {
		ERRORF(SITE_ERROR_MISSING_HEADERS, source_path);
		goto error;
	};

	// parse page content                                                       }
	size_t content_size = page_file->size - header_len;
	content = malloc(content_size + 1);
	if (content == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		goto error;
	}
	if ((page_parse_content(source_file, source_path, content_size, content)) != 0) {
		goto error;
	};

	// create whole Html file for page
	if (html_create_page(
		header, content, page_path, header_arr, is_blog, include_back_ref, include_title,
		include_date)
	    != 0) {
		goto error;
	};

	res->header = header;
	res->content = content;
	header = NULL; // transfer ownership; freed in main
	content = NULL; // transfer ownership; freed in main

	goto cleanup;

error:
	if (res)
		free(res);
	res = NULL;

cleanup:
	if (header) {
		free((char*)header->title);
		free(header);
	}
	if (content)
		free(content);
	if (source_file)
		fclose(source_file);

	return res;
}

int main(void)
{
	int res = 0;
	FTS* ftsp = NULL;
	FTSENT* ftsentp = NULL;
	char* path_prefix = NULL;

	page_header_arr header_arr = {
		.elems = { 0 },
		.len = 0,
	};

	if (__validate_ext_dirs(_SITE_EXT_GIT_DIR, _SITE_EXT_SOURCE_DIR) != 0) {
		res = -1;
		goto cleanup;
	}

	if ((path_prefix = __extract_ext_prefix(_SITE_EXT_GIT_DIR)) == NULL) {
		res = -1;
		goto cleanup;
	}

	if (__create_dir(_SITE_EXT_TARGET_DIR) != 0) {
		res = -1;
		goto cleanup;
	}

	if (html_init_templates() != 0) {
		res = -1;
		goto cleanup;
	}

	if (ghist_times(path_prefix)) {
		res = -1;
		goto cleanup;
	}

	if ((ftsp = __init_fts(_SITE_EXT_SOURCE_DIR)) == NULL) {
		res = -1;
		goto cleanup;
	}

	char curr_dir[PATH_MAX] = "\0";
	int curr_fts_level = 0;

	// Blog entry with owned string storage
	char index_name[256] = "\0";
	char index_path[PATH_MAX] = "\0";
	page_info index = { .name = index_name, .path = index_path, .size = 0 };
	bool found_index = false;

	while ((ftsentp = fts_read(ftsp)) != NULL) {
		if (curr_fts_level != ftsentp->fts_level) {
			// update current directory
			char* dir = __extract_dir(ftsentp->fts_path, false);
			snprintf(curr_dir, PATH_MAX, "%s", dir);

			free(dir);
			curr_fts_level = (int)ftsentp->fts_level;
		}

		if (ftsentp->fts_info == FTS_D) {
			// skip blocks
			if (ftsentp->fts_level == 1 && strcmp(ftsentp->fts_name, "blocks") == 0) {
				continue;
			}

			// obtain directory
			char* dir = __extract_dir(ftsentp->fts_path, true);

			// create current directory if it doesn't exist yet
			char target_dir[PATH_MAX];
			snprintf(target_dir, PATH_MAX, "%s/%s", _SITE_EXT_TARGET_DIR, dir);

			if (__create_dir(target_dir) != 0) {
				res = -1;
				goto cleanup;
			}

			free(dir);

		} else if (ftsentp->fts_info != FTS_F && ftsentp->fts_name[0] == '.') {
			// we only care for non-hidden files and directories
			continue;
		}

		char* dot = strrchr(ftsentp->fts_name, '.');
		if (!dot)
			continue;

		bool has_ext_htm = strcmp(dot + 1, "htm") == 0;

		if (!has_ext_htm) { // non-posts
			char to_path[_SITE_PATH_MAX];
			if (curr_dir[0] == '\0') {
				snprintf(
				    to_path, sizeof(to_path), "%s/%s", _SITE_EXT_TARGET_DIR,
				    ftsentp->fts_name);
			} else {
				snprintf(
				    to_path, sizeof(to_path), "%s/%s/%s", _SITE_EXT_TARGET_DIR,
				    curr_dir, ftsentp->fts_name);
			}

			__copy_file(ftsentp->fts_path, to_path);
		} else { // posts
			page_info page_file = {
				.name = ftsentp->fts_name,
				.path = ftsentp->fts_path,
				.size = ftsentp->fts_statp->st_size,
			};
			page_result* page_result;

			// skip blocks
			if (strcmp(ftsentp->fts_parent->fts_name, "blocks") == 0) {
				continue;
			}

			// skip drafts
			if (strcmp(ftsentp->fts_parent->fts_name, "drafts") == 0) {
				continue;
			}

			// ignore custom blog entry point
			if (strcmp(ftsentp->fts_name, _SITE_EXT_BLOG_INDEX) == 0) {
				strncpy(index_name, ftsentp->fts_name, 255);
				strncpy(index_path, ftsentp->fts_path, PATH_MAX - 1);

				index.size = ftsentp->fts_statp->st_size;

				found_index = true;

				continue;
			}

			if (header_arr.len >= _SITE_PAGES_MAX) {
				ERROR(SITE_ERROR_PAGE_NUMBER_EXCEEDED);
				res = -1;
				goto cleanup;
			}

			bool is_exempted = false;
			for (int i = 0; i < _SITE_EXEMPT_LIST_COUNT; i++) {
				if (strcmp(ftsentp->fts_name, index_excempt_arr[i]) == 0) {
					is_exempted = true;
					break;
				}
			}
			bool include_back_ref = !is_exempted;
			bool include_title = !is_exempted;
			bool include_date = strcmp(ftsentp->fts_name, "index.htm");

			if ((page_result = __process_page_file(
				 &page_file, curr_dir, &header_arr, false, include_back_ref,
				 include_title, include_date))
			    == NULL) {
				res = -1;
				goto cleanup;
			} else {
				// only add blog posts to the post list, not exempted pages
				if (!is_exempted) {
					header_arr.elems[header_arr.len] = page_result->header;
					header_arr.len++;

					content_arr.elems[content_arr.len] = page_result->content;
					content_arr.len++;
				}

				free(page_result);
			}
		}
	}

	// fill custom blog entry point with list of post entries
	if (found_index
	    && (__process_page_file(&index, curr_dir, &header_arr, true, false, false, false))
		== NULL) {
		res = -1;
		goto cleanup;
	}

	// Only create feed if there are blog posts
	if (header_arr.len > 0
	    && create_feed(
		   _SITE_EXT_TARGET_DIR "/"
					"feed.atom",
		   &header_arr)
		== -1) {
		res = -1;
		goto cleanup;
	}

cleanup:
	// cleanup
	if (ftsp)
		fts_close(ftsp);
	if (path_prefix)
		free(path_prefix);

	// headers (header_arr.elem allocated statically
	for (int i = 0; i < header_arr.len; i++) {
		free((char*)header_arr.elems[i]->title);
		free(header_arr.elems[i]);
	}

	// page content (content_arr.elem allocated statically
	for (int i = 0; i < content_arr.len; i++) {
		free(content_arr.elems[i]);
	}

	// tracked files (renamed files are to be cleaned
	for (int i = 0; i < tracked_arr.len; i++) {
		free(tracked_arr.files[i].file_path);
	}
	free(tracked_arr.files);

	// cleanup template invocations
	html_cleanup_templates();

	return res;
}
