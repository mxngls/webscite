#ifndef FEED_H

#include "config.h"
#include "html.h"
#include "page.h"

extern page_header_arr header_arr;
extern page_content_arr content_arr;

int create_feed(char *, page_header_arr *);

#endif // FEED_H
