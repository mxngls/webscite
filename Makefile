.POSIX:

SHELL = /bin/sh

_SITE_EXT_TARGET_DIR ?= docs/
_SITE_EXT_GIT_DIR ?= .git/

CC = clang

SRC_DIR = src/

LIBGIT2_VERSION = v1.9.0
LIBGIT2_DIR = deps/libgit2
LIBGIT2_BUILD = $(LIBGIT2_DIR)/build
LIBGIT2_LIB = $(LIBGIT2_BUILD)/libgit2.a

SYSTEM_LIBS = -lpthread -lz -lpcre -lssl -lcrypto

LDLIBS = $(LIBGIT2_LIB) $(SYSTEM_LIBS) 
LDFLAGS = -L/usr/local/lib

CFLAGS = \
-std=c99 \
-Wall \
-Wextra \
-Wconversion \
-Wno-sign-conversion \
-Wdouble-promotion \
-Werror \
-Wpedantic \
-Wpointer-arith \
-D_SITE_EXT_TARGET_DIR=\"$(_SITE_EXT_TARGET_DIR)\" \
-D_SITE_EXT_GIT_DIR=\"$(_SITE_EXT_GIT_DIR)\" \
-I$(LIBGIT2_DIR)/include

DEBUG_CFLAGS = $(CFLAGS) \
--debug \
-fsanitize=address,undefined

# deploy
deploy: clean build

# debug build target
debug: $(LIBGIT2_LIB) $(SRC_DIR)/*.c
	@printf "%s\n" "Building site generator (DEBUG)..."
	@$(CC) $(LDFLAGS) $(DEBUG_CFLAGS) $(SRC_DIR)/*.c -o main.out $(LDLIBS)
	@printf "%s\n" "Generating pages (DEBUG)..."
	@./main.out

#build
build: $(LIBGIT2_LIB) $(SRC_DIR)/*.c
	@printf "%s\n" "Building site generator..."
	@$(CC) $(LDFLAGS) $(CFLAGS) $(SRC_DIR)/*.c -o main.out $(LDLIBS)
	@printf "%s\n" "Generating pages..."
	@./main.out

# download and build libgit2
$(LIBGIT2_LIB):
	@printf "%s\n" "Setting up libgit2..."
	@mkdir -p deps
	@if [ ! -d "$(LIBGIT2_DIR)" ]; then \
		printf "%s\n" "Downloading libgit2 $(LIBGIT2_VERSION)..."; \
		wget -q https://github.com/libgit2/libgit2/archive/$(LIBGIT2_VERSION).tar.gz --output-document deps/libgit2.tar.gz; \
		tar -xzf deps/libgit2.tar.gz -C deps; \
		mv deps/libgit2-* $(LIBGIT2_DIR); \
		rm deps/libgit2.tar.gz; \
	fi
	@mkdir -p $(LIBGIT2_BUILD)
	@cd $(LIBGIT2_BUILD) && cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
	@cd $(LIBGIT2_BUILD) && cmake --build .
	@printf "%s\n" "libgit2 setup complete."

# clean the build directory
clean:
	@printf "%s\n" "Removing build artifacts..."
	@if [ -d "$(_SITE_EXT_TARGET_DIR)" ]; then find "$(_SITE_EXT_TARGET_DIR)" -mindepth 1 -delete; fi
	@if [ -f "main.out" ]; then rm main.out; fi
	@rm -f build.o

# deep clean including dependencies
distclean: clean
	@printf "%s\n" "Removing dependencies..."
	@rm -rf deps
	
.PHONY: build clean deploy distclean debug
