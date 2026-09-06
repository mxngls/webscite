#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

#define ARRAY_GROWTH_FACTOR    2
#define ARRAY_DEFAULT_CAPACITY 100

#define ARRAY_PUSH(array, item, on_fail)                                                           \
	do {                                                                                       \
		if ((array)->len == (array)->capacity) {                                           \
			void* grown = array_grow(                                                  \
			    (array)->items, &(array)->capacity, sizeof *(array)->items);           \
			if (!grown) {                                                              \
				on_fail;                                                           \
			}                                                                          \
			(array)->items = grown;                                                    \
		}                                                                                  \
		(array)->items[(array)->len] = (item);                                             \
		(array)->len++;                                                                    \
	} while (0)

void* array_grow(void* items, int* capacity, size_t item_size);

#endif
