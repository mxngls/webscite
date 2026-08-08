// shared config

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#ifndef _SITE_EXT_SOURCE_DIR
#define _SITE_EXT_SOURCE_DIR "content"
#endif

#ifndef _SITE_EXT_TARGET_DIR
#define _SITE_EXT_TARGET_DIR "docs"
#endif

#ifndef _SITE_EXT_BLOG_INDEX
#define _SITE_EXT_BLOG_INDEX "index.htm"
#endif

#ifndef _SITE_EXT_GIT_DIR
#define _SITE_EXT_GIT_DIR ".git"
#endif

#ifndef _SITE_EXT_TITLE
#error "_SITE_EXT_TITLE not defined."
#endif

#ifndef _SITE_EXT_AUTHOR
#error "_SITE_EXT_AUTHOR not defined."
#endif

#ifndef _SITE_EXT_FEED_ID
#error "_SITE_EXT_FEED_ID not defined."
#endif

#ifndef _SITE_EXT_HOST
#error "_SITE_EXT_HOST not defined."
#endif

#ifndef _SITE_EXT_URL
#define _SITE_EXT_URL "https://"_SITE_EXT_HOST
#endif

#ifndef _SITE_EXT_TAG_SCHEME_DATE
#error "_SITE_EXT_TAG_SCHEME_DATE not defined."
#endif

#endif // CONFIG_H
