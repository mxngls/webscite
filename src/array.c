#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"

void* array_grow(void* items, int* capacity, size_t item_size)
{
	int future_capacity = *capacity ? *capacity * ARRAY_GROWTH_FACTOR : ARRAY_DEFAULT_CAPACITY;
	void* grown = realloc(items, future_capacity * item_size);
	if (!grown) {
		return NULL;
	}
	*capacity = future_capacity;
	return grown;
}
