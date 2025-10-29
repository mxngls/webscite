#ifndef GHIST_H
#define GHIST_H

#include <git2.h>

typedef struct {
        char *file_path;
        git_time_t creat_time;
        git_time_t mod_time;
} tracked_file;

typedef struct {
        tracked_file *files;
        int len;
        int capacity;
} tracked_file_arr;

extern tracked_file_arr tracked_arr;

typedef struct {
        git_signature *signature;
        char *path_prefix;
} diff_cb_payload;

// obtain modification and creation times
int ghist_times(char *);
void ghist_format_ts(char *, char *, time_t);

// match tracked files and files residing in the working dir
tracked_file *ghist_find_by_path(char *);

#endif // GHIST_H
