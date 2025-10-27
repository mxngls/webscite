#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "error.h"
#include "ghist.h"

typedef struct {
        char *old_path;
        char *new_path;
        git_time_t creat_time;
} rename_record;

typedef struct {
        rename_record *records;
        int len;
        int capacity;
} renamed_file_arr;

static renamed_file_arr rename_arr = {0};

static void __add_rename(char *old_path, char *new_path, git_time_t timestamp) {
        if (rename_arr.records == NULL) {
                rename_arr.records = malloc(sizeof(rename_record) * 100);
                rename_arr.capacity = 100;
        } else if (rename_arr.capacity == rename_arr.len) {
                rename_arr.capacity *= 2;
                rename_arr.records =
                    realloc(rename_arr.records, rename_arr.capacity * sizeof(rename_record));
        }

        rename_arr.records[rename_arr.len] = (rename_record){
            .old_path = strdup(old_path),
            .new_path = strdup(new_path),
            .creat_time = timestamp,
        };
        rename_arr.len++;
}

static void __trace_rename(char *final_path, git_time_t *creation_time,
                           git_time_t *modification_time) {

        char *current_path = strdup(final_path);

        for (int i = rename_arr.len - 1; i >= 0; i--) {
                if (strcmp(rename_arr.records[i].new_path, current_path) != 0) continue;

                *modification_time =
                    *modification_time == 0 ? rename_arr.records[i].creat_time : *modification_time;
                *creation_time = rename_arr.records[i].creat_time;

                // free old path name
                free(current_path);

                current_path = strdup(rename_arr.records[i].old_path);

                i = rename_arr.len;
        }

        free(current_path);
}

static int __get_times_cb(const git_diff_delta *delta, __attribute__((unused)) float progress,
                          void *payload) {
        if (!delta || !delta->new_file.path) return 0;

        // ensure array capacity
        if (tracked_arr.files == NULL) {
                tracked_arr.files = malloc(sizeof(tracked_file) * 100);
                tracked_arr.capacity = 100;
        } else if (tracked_arr.capacity == tracked_arr.len) {
                tracked_arr.capacity *= 2;
                tracked_arr.files =
                    realloc(tracked_arr.files, tracked_arr.capacity * sizeof(tracked_file));
                if (!tracked_arr.files) return -1;
        }

        char *file_path = (char *)delta->new_file.path;
        char *old_file_path = (char *)delta->old_file.path;

        git_signature *signature = (git_signature *)payload;
        git_time_t author_time = signature->when.time;

        // rename detected
        if (delta->similarity > 50 && strcmp(old_file_path, file_path) != 0) {
                __add_rename(old_file_path, file_path, author_time);

                if (access(file_path, F_OK) == 0 && !ghist_find_by_path(file_path)) {
                        tracked_file new_file = {
                            .file_path = strdup(file_path),
                            .creat_time = author_time,
                            .mod_time = author_time,
                        };
                        tracked_arr.files[tracked_arr.len] = new_file;
                        tracked_arr.len++;
                }
                return 0;
        }

        // regular file change
        if (access(file_path, F_OK) != 0) return 0;

        tracked_file *tracked = ghist_find_by_path(file_path);
        if (tracked) {
                tracked->mod_time = author_time;
                return 0;
        }

        // new file
        tracked_file new_file = {
            .file_path = strdup(file_path),
            .creat_time = author_time,
            .mod_time = 0,
        };

        tracked_arr.files[tracked_arr.len] = new_file;
        tracked_arr.len++;

        return 0;
}

void ghist_format_ts(char *format_str, char *formatted, time_t timestamp) {
        time_t time = (time_t)timestamp;
        struct tm tm;
        if (gmtime_r(&time, &tm)) {
                strftime(formatted, 256, format_str, &tm);
        } else {
                strcpy(formatted, "Invalid date");
        }
}

tracked_file *ghist_find_by_path(char *file_path) {
        for (int i = 0; i < tracked_arr.len; i++) {
                if (strcmp(tracked_arr.files[i].file_path, file_path) == 0) {
                        return &tracked_arr.files[i];
                }
        }
        return NULL;
}

int ghist_times(void) {
        int res = 0;

        git_libgit2_init();

        git_oid oid;
        git_repository *repo = NULL;
        git_revwalk *walker = NULL;
        git_commit *commit = NULL;
        git_commit *parent = NULL;
        git_tree *tree = NULL;
        git_tree *parent_tree = NULL;
        git_diff *diff = NULL;

        if (git_repository_open(&repo, _SITE_EXT_GIT_DIR) != 0) goto error;
        if (git_revwalk_new(&walker, repo)) goto error;
        if (git_revwalk_sorting(walker, GIT_SORT_TIME | GIT_SORT_REVERSE)) goto error;
        if (git_revwalk_push_head(walker)) goto error;

        while (git_revwalk_next(&oid, walker) == 0) {
                // free previously allocted resources
                // clang-format off
                if (commit) { git_commit_free(commit); commit = NULL; }
                if (parent) { git_commit_free(parent); parent = NULL; }
                if (tree) { git_tree_free(tree); tree = NULL; }
                if (parent_tree) { git_tree_free(parent_tree); parent_tree = NULL; }
                if (diff) { git_diff_free(diff); diff = NULL; }
                // clang-format on

                if (git_commit_lookup(&commit, repo, &oid)) goto error;

                int parent_count = git_commit_parentcount(commit);
                if (parent_count != 1) continue;

                if (git_commit_parent(&parent, commit, 0)) goto error;
                if (git_commit_tree(&tree, commit)) goto error;
                if (git_commit_tree(&parent_tree, parent)) goto error;
                if (git_diff_tree_to_tree(&diff, repo, parent_tree, tree, NULL)) goto error;

                // enable dection of renamed files
                git_diff_find_options *find_opts = malloc(sizeof(git_diff_find_options));
                if (git_diff_find_options_init(find_opts, GIT_DIFF_FIND_OPTIONS_VERSION))
                        goto error;
                find_opts->flags = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_IGNORE_WHITESPACE;
                if (git_diff_find_similar(diff, find_opts)) goto error;

                git_signature *signature = (git_signature *)git_commit_author(commit);
                if (git_diff_foreach(diff, &__get_times_cb, NULL, NULL, NULL, (void *)signature))
                        goto error;
        }

        // resolve renames
        for (int i = 0; i < tracked_arr.len; i++) {
                git_time_t creation_time = 0;
                git_time_t last_rename_time = 0;
                __trace_rename(tracked_arr.files[i].file_path, &creation_time, &last_rename_time);
                if (creation_time > 0) {
                        tracked_arr.files[i].creat_time = creation_time;
                }
                if (last_rename_time > 0) {
                        tracked_arr.files[i].mod_time = last_rename_time;
                }
        }

        goto cleanup;

error:
        res = -1;
        git_error *err = (git_error *)git_error_last();
        ERRORF(SITE_ERROR_GIT_OPERATION, err->message);

cleanup:
        git_repository_free(repo);
        git_revwalk_free(walker);
        git_commit_free(commit);
        git_commit_free(parent);
        git_tree_free(tree);
        git_tree_free(parent_tree);

        for (int i = 0; i < rename_arr.len; i++) {
                free(rename_arr.records[i].old_path);
                free(rename_arr.records[i].new_path);
        }
        free(rename_arr.records);

        return res;
}
