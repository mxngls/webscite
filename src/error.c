#include "error.h"

const char *get_error_format(site_error_t error) {
        // clang-format off
        switch (error) {
	case SITE_SUCCESS:			return "Success";
	case SITE_ERROR_FILE_OPEN_READ:		return "Failed to open source %s";
	case SITE_ERROR_FILE_OPEN_WRITE:	return "Failed to open destination %s";
	case SITE_ERROR_FILE_READ:		return "Failed to read from source %s";
	case SITE_ERROR_FILE_WRITE:		return "Failed to write to file";
	case SITE_ERROR_FILE_CREATE:		return "Failed to create file %s";
	case SITE_ERROR_FILE_STAT:		return "Failed to stat file: %s";
	case SITE_ERROR_FILE_SIZE_MISMATCH:	return "Read %zu bytes, expected %jd";
	case SITE_ERROR_FILE_SEEK:		return "Couldn't seek in file: %s";
	case SITE_ERROR_FILE_TELL:		return "Couldn't tell position of  in file: %s";
	case SITE_ERROR_UNEXPECTED_EOF:		return "Unexpected EOF. Read %zu bytes, expected %jd";
	
	case SITE_ERROR_MEMORY_ALLOCATION:	return "Memory allocation failed";
						
	case SITE_ERROR_DIRECTORY_CREATE:	return "Failed to create directory";
	
	case SITE_ERROR_FTS_INIT:		return "Failed to initialize file tree walk";
	
	case SITE_ERROR_MISSING_HEADERS:	return "Title and subtitle headers missing: %s";
	case SITE_ERROR_EMPTY_CONTENT:		return "Page has no content. Aborting.";
	case SITE_ERROR_NO_PAGES_FOUND:		return "No pages to convert. Aborting";
	
	case SITE_ERROR_GIT_OPERATION:		return "Git operation failed";
	default:				return "Unknown error";
        }
        // clang-format on
}
