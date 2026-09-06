#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "ghist.h"
#include "git2/odb.h"

typedef struct {
	char* old_path;
	char* new_path;
	git_time_t rename_time;
} rename_record;

typedef struct {
	rename_record* records;
	int len;
	int capacity;
} renamed_file_arr;

static renamed_file_arr renamed_files = { 0 };

static int __add_rename(char* old_path, char* new_path, git_time_t timestamp)
{
	if (renamed_files.records == NULL) {
		renamed_files.records = malloc(sizeof(rename_record) * 100);
		renamed_files.capacity = 100;
	} else if (renamed_files.capacity == renamed_files.len) {
		renamed_files.capacity *= 2;
		rename_record* grown = realloc(
		    renamed_files.records, renamed_files.capacity * sizeof(rename_record));
		if (!grown) {
			ERROR(SITE_ERROR_MEMORY_ALLOCATION);
			return -1;
		}
		renamed_files.records = grown;
	}

	renamed_files.records[renamed_files.len] = (rename_record) {
		.old_path = strdup(old_path),
		.new_path = strdup(new_path),
		.rename_time = timestamp,
	};
	renamed_files.len++;

	return 0;
}

static int __trace_rename(
    char* final_path,
    git_time_t* creation_time,
    git_time_t* modification_time,
    tracked_file_arr* tracked_files)
{

	char* current_path = strdup(final_path);

	// Walk every rename entry; TODO: a hash map would obviously be better here	than the
	// current O(n^2) based lookup
	for (int i = renamed_files.len - 1; i >= 0; i--) {
		if (strcmp(renamed_files.records[i].new_path, current_path) != 0)
			continue;

		*modification_time = *modification_time == 0 ? renamed_files.records[i].rename_time
							     : *modification_time;

		// free old path name
		free(current_path);

		current_path = strdup(renamed_files.records[i].old_path);

		i = renamed_files.len;
	}

	// resolve initial creation
	tracked_file* final_entry = NULL;
	if ((final_entry = ghist_find_by_path(current_path, tracked_files)) == NULL) {
		fprintf(stderr, "Error: no matching tracked entry found for: %s\n", current_path);
		free(current_path);
		return -1;
	}

	*creation_time = final_entry->creat_time;

	free(current_path);

	return 0;
}

static int __get_times_cb(
    const git_diff_delta* delta,
    __attribute__((unused)) float progress,
    void* payload)
{
	if (!delta || !delta->new_file.path)
		return 0;

	diff_cb_payload* cb_payload = (diff_cb_payload*)payload;
	tracked_file_arr* tracked_files = cb_payload->tracked_files;

	// ensure array capacity
	if (tracked_files->files == NULL) {
		tracked_files->files = malloc(sizeof(tracked_file) * 100);
		tracked_files->capacity = 100;
	} else if (tracked_files->capacity == tracked_files->len) {
		tracked_files->capacity *= 2;
		tracked_file* grown
		    = realloc(tracked_files->files, tracked_files->capacity * sizeof(tracked_file));
		if (!grown) {
			ERROR(SITE_ERROR_MEMORY_ALLOCATION);
			return -1;
		}
		tracked_files->files = grown;
	}

	char file_path[PATH_MAX] = { '\0' };
	char old_file_path[PATH_MAX] = { '\0' };
	snprintf(
	    file_path, sizeof(file_path), "%s%s", cb_payload->path_prefix, delta->new_file.path);
	snprintf(
	    old_file_path, sizeof(old_file_path), "%s%s", cb_payload->path_prefix,
	    delta->old_file.path);

	git_signature* signature = cb_payload->signature;
	git_time_t author_time = signature->when.time;

	// rename detected
	if (delta->similarity > 50 && strcmp(old_file_path, file_path) != 0) {
		if (__add_rename(old_file_path, file_path, author_time)) {
			return -1;
		};

		if (access(file_path, F_OK) == 0 && !ghist_find_by_path(file_path, tracked_files)) {
			tracked_file new_file = {
				.file_path = strdup(file_path),
				.creat_time = author_time,
				.mod_time = author_time,
			};
			tracked_files->files[tracked_files->len] = new_file;
			tracked_files->len++;
		}
		return 0;
	}

	tracked_file* tracked = ghist_find_by_path(file_path, tracked_files);
	if (tracked) {
		tracked->mod_time = author_time;
		return 0;
	}

	// new file or initial version of renamed revision
	tracked_file new_file = {
		.file_path = strdup(file_path),
		.creat_time = author_time,
		.mod_time = 0,
	};

	tracked_files->files[tracked_files->len] = new_file;
	tracked_files->len++;

	return 0;
}

void ghist_format_ts(char* format_str, char* formatted, time_t timestamp)
{
	time_t time = (time_t)timestamp;
	struct tm tm;
	if (gmtime_r(&time, &tm)) {
		strftime(formatted, 256, format_str, &tm);
	} else {
		strcpy(formatted, "Invalid date");
	}
}

tracked_file* ghist_find_by_path(char* file_path, tracked_file_arr* tracked_files)
{
	for (int i = 0; i < tracked_files->len; i++) {
		if (strcmp(tracked_files->files[i].file_path, file_path) == 0) {
			return &tracked_files->files[i];
		}
	}
	return NULL;
}

int ghist_times(char* path_prefix, tracked_file_arr* tracked_files)
{
	int res = 0;

	git_libgit2_init();

	git_oid oid;
	git_repository* repo = NULL;
	git_revwalk* walker = NULL;
	git_commit* commit = NULL;
	git_commit* parent = NULL;
	git_tree* tree = NULL;
	git_tree* parent_tree = NULL;
	git_diff* diff = NULL;
	git_diff_find_options* find_opts = NULL;

	if (git_repository_open(&repo, _SITE_EXT_GIT_DIR) != 0)
		goto git_error;
	if (git_revwalk_new(&walker, repo))
		goto git_error;
	if (git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_REVERSE))
		goto git_error;
	if (git_revwalk_push_head(walker))
		goto git_error;

	while (git_revwalk_next(&oid, walker) == 0) {
		// free previously allocted resources
		// clang-format off
                if (commit) { git_commit_free(commit); commit = NULL; }
                if (parent) { git_commit_free(parent); parent = NULL; }
                if (tree) { git_tree_free(tree); tree = NULL; }
                if (parent_tree) { git_tree_free(parent_tree); parent_tree = NULL; }
                if (diff) { git_diff_free(diff); diff = NULL; }
                if (find_opts) { free(find_opts); find_opts = NULL; }
		// clang-format on

		if (git_commit_lookup(&commit, repo, &oid))
			goto git_error;

		if (git_commit_tree(&tree, commit))
			goto git_error;

		int parent_count = git_commit_parentcount(commit);

		// skip merge commits ...
		if (parent_count > 1) {
			continue;
		} // ... but process root commits
		else if (parent_count == 1) {
			if (git_commit_parent(&parent, commit, 0))
				goto git_error;
			if (git_commit_tree(&parent_tree, parent))
				goto git_error;
		}

		if (git_diff_tree_to_tree(&diff, repo, parent_tree, tree, NULL))
			goto git_error;

		find_opts = malloc(sizeof(git_diff_find_options));
		if (git_diff_find_options_init(find_opts, GIT_DIFF_FIND_OPTIONS_VERSION)) {
			goto git_error;
		}

		// enable dection of renamed files and ignore whitespace changes
		find_opts->flags = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_IGNORE_WHITESPACE;

		// resovle renames, copies etc.
		if (git_diff_find_similar(diff, find_opts)) {
			goto git_error;
		}

		// iterate over individual diffs
		git_signature* signature = (git_signature*)git_commit_author(commit);
		if (git_diff_foreach(
			diff, &__get_times_cb, NULL, NULL, NULL,
			(void*)&(diff_cb_payload) {
			    .signature = signature,
			    .path_prefix = path_prefix,
			    .tracked_files = tracked_files,
			})) {
			goto git_error;
		}
	}

	// resolve renames
	for (int i = 0; i < tracked_files->len; i++) {
		git_time_t creation_time = 0;
		git_time_t last_rename_time = 0;
		if (__trace_rename(
			tracked_files->files[i].file_path, &creation_time, &last_rename_time,
			tracked_files)) {
			goto error;
		};
		if (creation_time > 0) {
			tracked_files->files[i].creat_time = creation_time;
		}
		if (last_rename_time > 0) {
			tracked_files->files[i].mod_time = last_rename_time;
		}
	}

	goto cleanup;

git_error:
	res = -1;
	git_error* err = (git_error*)git_error_last();
	ERRORF(SITE_ERROR_GIT_OPERATION, err->message);

error:
	res = -1;

cleanup:
	git_repository_free(repo);
	git_revwalk_free(walker);

	git_commit_free(commit);
	git_tree_free(tree);

	if (parent)
		git_commit_free(parent);
	if (parent_tree)
		git_tree_free(parent_tree);

	git_diff_free(diff);

	if (find_opts) {
		free(find_opts);
	}

	for (int i = 0; i < renamed_files.len; i++) {
		// Free both old_path and new_path since strdup() created copies
		free(renamed_files.records[i].old_path);
		free(renamed_files.records[i].new_path);
	}
	free(renamed_files.records);
	renamed_files = (renamed_file_arr) { 0 };

	return res;
}

// obtain blob hash
int ghist_blob_hash(char* hash, size_t hash_len, char* file_path)
{
	git_error* err = NULL;
	git_oid oid;

	if (git_odb_hashfile(&oid, file_path, GIT_OBJECT_BLOB)) {
		err = (git_error*)git_error_last();
		goto error;
	}
	if (!git_oid_tostr(hash, hash_len, &oid)) {
		err = (git_error*)git_error_last();
		goto error;
	}

error:
	if (err) {
		char err_msg[256];
		snprintf(
		    err_msg, sizeof(err_msg), "%d | Received file_path parameter: %s\n%s",
		    err->klass, file_path, err->message);
		ERRORF(SITE_ERROR_GIT_OPERATION, err_msg);
		return -1;
	}

	return 0;
}
