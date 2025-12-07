#!/bin/bash

# macOS-specific build wrapper
# Uses frameworks instead of system libraries for OpenSSL/etc.

make SYSTEM_LIBS='-lz -liconv -framework CoreFoundation -framework Security' "$@"
