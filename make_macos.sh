#!/bin/bash

# needed when using -fsanitize=address
export MallocNanoZone=0

# overwrite defaults set for FreeBSD when working locally on macOS
make SYSTEM_LIBS='-lz -liconv -framework CoreFoundation -framework Security' "$@"
