#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.h"
#include "feed.h"
#include "ghist.h"
#include "html.h"
#include "page.h"

typedef struct {
	char* name;
	char* path;
	off_t size;
} page_info;

// main routine and associated function(s)
static page_entry* __process_page_file(page_info*, char*, tracked_file_arr*);
static void page_entry_free(page_entry** e);

static void __join_path(char* dst, size_t size, char* parent, char* child)
{
	if (parent[0] == '\0') {
		snprintf(dst, size, "%s", child);
	} else if (child[0] == '\0') {
		snprintf(dst, size, "%s", parent);
	} else {
		snprintf(dst, size, "%s/%s", parent, child);
	}
}

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
		ERRORF(SITE_ERROR_FILE_READ, from);
		res = -1;
	}

	free(line);
	fclose(from_file);
	fclose(to_file);

	return res;
}

static void page_entry_free(page_entry** e)
{

	if (e == NULL || *e == NULL) {
		return;
	}

	// free individual fields first
	free((char*)(*e)->headers.title);
	free((*e)->content);

	free(*e);
	*e = NULL;
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

	if ((git_path_copy = malloc(PATH_MAX - 1)) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		res = -1;
		goto cleanup;
	};
	strncpy(git_path_copy, git_absolute_path, PATH_MAX - 1);

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

static int __process_dir(char* sub_dir, page_entry_arr* entry_arr, tracked_file_arr* tracked_files)
{
	int res = 0;

	DIR* dirp = NULL;
	struct dirent* dir_ent = NULL;

	// filesystem path for this directory, relative to the repository root;
	char src_dir[PATH_MAX];
	__join_path(src_dir, sizeof(src_dir), _SITE_EXT_SOURCE_DIR, sub_dir);

	if ((dirp = opendir(src_dir)) == NULL) {
		fprintf(stderr, "Failed to open directory: %s\n", src_dir);
		goto error;
	}

	while ((dir_ent = readdir(dirp)) != NULL) {

		char src_file[PATH_MAX] = "";

		// ignore current dir
		if (strncmp(".", dir_ent->d_name, 1) == 0) {
			continue;
		}

		__join_path(src_file, sizeof(src_file), src_dir, dir_ent->d_name);

		struct stat statbuf;
		if (lstat(src_file, &statbuf) == -1) {
			fprintf(stderr, "Failed to stat '%s' - %s\n", src_file, strerror(errno));
			goto error;
		}

		if (S_ISDIR(statbuf.st_mode)) {
			if (strcmp(dir_ent->d_name, "blocks") == 0
			    || strcmp(dir_ent->d_name, "drafts") == 0) {
				continue;
			}

			char child_sub_dir[PATH_MAX];
			__join_path(child_sub_dir, sizeof(child_sub_dir), sub_dir, dir_ent->d_name);

			// create target directory if it doesn't exist yet
			char out_dir[PATH_MAX];
			__join_path(out_dir, sizeof(out_dir), _SITE_EXT_TARGET_DIR, child_sub_dir);
			if (__create_dir(out_dir) != 0) {
				goto error;
			}

			if (__process_dir(child_sub_dir, entry_arr, tracked_files) == -1) {
				goto error;
			}

			continue;
		}

		if (!S_ISREG(statbuf.st_mode)) {
			continue;
		}

		char* dot = strrchr(dir_ent->d_name, '.');
		if (!dot) {
			continue;
		}

		if (strcmp(dot + 1, "htm") != 0) {
			// non-posts
			char out_dir[PATH_MAX];
			char out_file[PATH_MAX];
			__join_path(out_dir, sizeof(out_dir), _SITE_EXT_TARGET_DIR, sub_dir);
			__join_path(out_file, sizeof(out_file), out_dir, dir_ent->d_name);

			if (__copy_file(src_file, out_file)) {
				goto error;
			}
		} else { // posts
			page_info page_file = {
				.name = dir_ent->d_name,
				.path = src_file,
				.size = statbuf.st_size,
			};
			page_entry* entry_res;

			if (entry_arr->len >= _SITE_PAGES_MAX) {
				ERROR(SITE_ERROR_PAGE_NUMBER_EXCEEDED);
				goto error;
			}

			if ((entry_res = __process_page_file(&page_file, sub_dir, tracked_files))
			    == NULL) {
				goto error;
			} else {
				entry_arr->elems[entry_arr->len] = entry_res;
				entry_arr->len++;
			}
		}
	}

	goto cleanup;

error:
	res = -1;

cleanup:
	if (dirp) {
		closedir(dirp);
	}

	return res;
}

static page_entry* __process_page_file(
    page_info* page_file,
    char* curr_dir,
    tracked_file_arr* tracked_files)
{
	page_entry* entry_res = NULL;
	page_entry* entry = NULL;
	char* content = NULL;
	char* source_path = page_file->path;
	FILE* source_file = NULL;
	tracked_file* tracked = NULL;

	if ((source_file = fopen(page_file->path, "r")) == NULL) {
		ERRORF(SITE_ERROR_FILE_READ, source_path);
		goto error;
	}

	// convert extension to proper .html
	char page_name[256] = "\0";
	snprintf(page_name, sizeof(page_name), "%s", page_file->name);
	strlcat(page_name, "l", sizeof(page_name));

	// output path
	char page_path[PATH_MAX];
	if (curr_dir[0] == '\0') {
		snprintf(page_path, sizeof(page_path), "%s/%s", _SITE_EXT_TARGET_DIR, page_name);
	} else {
		snprintf(
		    page_path, sizeof(page_path), "%s/%s/%s", _SITE_EXT_TARGET_DIR, curr_dir,
		    page_name);
	}

	// handle page entries
	if ((entry = calloc(1, sizeof(page_entry))) == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		goto error;
	}
	strncpy(entry->meta.source_path, source_path, PATH_MAX - 1);

	// everything's a post by default
	entry->headers.is_post = true;
	entry->headers.include_header = false;
	entry->headers.include_footer = false;
	entry->headers.include_title = true;
	entry->headers.include_date = true;

	// populate page metadata
	char page_href[100];
	if (curr_dir[0] == '\0') {
		snprintf(page_href, sizeof(page_href), "/%s", page_name);
	} else {
		snprintf(page_href, sizeof(page_href), "%s/%s", curr_dir, page_name);
	}
	strncpy(entry->meta.path, page_href, PATH_MAX - 1);
	if ((tracked = ghist_find_by_path(source_path, tracked_files))) {
		entry->meta.created = tracked->creat_time;
		entry->meta.modified = tracked->mod_time;

		if (ghist_blob_hash(
			entry->meta.hash, sizeof(entry->meta.hash), tracked->file_path)) {
			goto error;
		}
	}

	// parse page header
	int entry_len = -1;
	if ((entry_len = page_parse_header(source_file, entry)) == -1) {
		goto error;
	};

	// parse page content
	if (entry_len < 0 || (off_t)entry_len > page_file->size) {
		ERROR(SITE_ERROR_EMPTY_CONTENT);
		goto error;
	}
	size_t content_size = (size_t)(page_file->size - entry_len);
	content = malloc(content_size + 1);
	if (content == NULL) {
		ERROR(SITE_ERROR_MEMORY_ALLOCATION);
		goto error;
	}
	entry->content = content;
	if ((page_parse_content(source_file, source_path, content_size, entry->content)) != 0) {
		goto error;
	};

	// create whole Html file for page
	if (html_create_page(entry, content, page_path) != 0) {
		goto error;
	};

	entry_res = entry;

	// transfer ownership; freed in main
	entry = NULL;

	goto cleanup;

error:
	entry_res = NULL;

cleanup:
	if (entry) {
		free(entry->headers.title);
		free(entry->headers.class);
		free(entry->content);
		free(entry);
	}
	if (source_file) {
		fclose(source_file);
	}

	return entry_res;
}

int main(void)
{
	int res = 0;

	char* path_prefix = NULL;

	page_entry_arr entry_arr = {
		.elems = { 0 },
		.len = 0,
	};

	tracked_file_arr tracked_files = {
		.files = NULL,
		.len = 0,
		.capacity = 0,
	};

	if (__validate_ext_dirs(_SITE_EXT_GIT_DIR, _SITE_EXT_SOURCE_DIR) != 0) {
		goto error;
	}

	if ((path_prefix = __extract_ext_prefix(_SITE_EXT_GIT_DIR)) == NULL) {
		goto error;
	}

	if (__create_dir(_SITE_EXT_TARGET_DIR) != 0) {
		goto error;
	}

	if (html_init_templates() != 0) {
		goto error;
	}

	if (ghist_times(path_prefix, &tracked_files)) {
		goto error;
	}

	if (__process_dir("", &entry_arr, &tracked_files) == -1) {
		goto error;
	}

	if (entry_arr.len == 0) {
		ERROR(SITE_ERROR_NO_PAGES_FOUND);
		goto error;
	}

	// Only create feed if there are blog posts
	if (entry_arr.len > 1
	    && create_feed(
		   _SITE_EXT_TARGET_DIR "/"
					"feed.atom",
		   &entry_arr)
		== -1) {
		goto error;
	}

	goto cleanup;

error:
	res = -1;

cleanup:
	// cleanup
	if (path_prefix)
		free(path_prefix);

	// entries (entry_arr.elem) allocated statically
	for (int i = 0; i < entry_arr.len; i++) {
		page_entry_free(&entry_arr.elems[i]);
	}

	// tracked files (renamed files are to be cleaned
	for (int i = 0; i < tracked_files.len; i++) {
		free(tracked_files.files[i].file_path);
	}
	free(tracked_files.files);

	// cleanup template invocations
	html_cleanup_templates();

	return res;
}
