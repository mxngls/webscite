#ifndef ERROR_H

typedef enum {
        SITE_SUCCESS = 0,

        // file I/O errors
        SITE_ERROR_FILE_OPEN_READ,
        SITE_ERROR_FILE_OPEN_WRITE,
        SITE_ERROR_FILE_READ,
        SITE_ERROR_FILE_WRITE,
        SITE_ERROR_FILE_CREATE,
        SITE_ERROR_FILE_STAT,
        SITE_ERROR_FILE_SIZE_MISMATCH,
        SITE_ERROR_FILE_SEEK,
        SITE_ERROR_FILE_TELL,
        SITE_ERROR_UNEXPECTED_EOF,

        // memory allocation errors
        SITE_ERROR_MEMORY_ALLOCATION,

        // directory operations
        SITE_ERROR_DIRECTORY_CREATE,

        // file system traversal
        SITE_ERROR_FTS_INIT,

        // content validation
        SITE_ERROR_MISSING_HEADERS,
        SITE_ERROR_EMPTY_CONTENT,
        SITE_ERROR_NO_PAGES_FOUND,

        // Git operations
        SITE_ERROR_GIT_OPERATION
} site_error_t;

const char *get_error_format(site_error_t error);

#define ERROR(error_type)                                                                          \
        if (errno != 0) {                                                                          \
                fprintf(stderr, "Error: %s (errno: %d, %s, %s:%d)\n",                              \
                        get_error_format(error_type), errno, strerror(errno), __FILE__, __LINE__); \
        } else {                                                                                   \
                fprintf(stderr, "Error: %s (%s:%d)\n", get_error_format(error_type), __FILE__,     \
                        __LINE__);                                                                 \
        }

#define ERRORF(error_type, ...)                                                                    \
        if (errno != 0) {                                                                          \
                fprintf(stderr, get_error_format(error_type), __VA_ARGS__);                        \
                fprintf(stderr, " (errno: %d, %s, %s:%d)\n", errno, strerror(errno), __FILE__,     \
                        __LINE__);                                                                 \
        } else {                                                                                   \
                fprintf(stderr, get_error_format(error_type), __VA_ARGS__);                        \
                fprintf(stderr, " (%s:%d)\n", __FILE__, __LINE__);                                 \
        }

#endif // ERROR_H
