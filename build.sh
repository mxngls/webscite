#!/bin/bash

# Common arguments
ARGS=(
	_SITE_EXT_TITLE="'Max'\''s Homepage'"
)

# On macOS, append system libraries
if [[ "$OSTYPE" == "darwin"* ]]; then
	ARGS+=(SYSTEM_LIBS='-lz -liconv -framework CoreFoundation -framework Security')
fi

# Run make with all arguments
make "${ARGS[@]}" "$@"
