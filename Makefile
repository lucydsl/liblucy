# Every build, native and wasm, is C17 with these warnings, and builds
# without any. WERROR=1 makes them errors, as CI does.
CSTD=-std=c17
WARNINGS=-Wall -Wextra -Wpedantic \
	-Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition \
	-Wshadow -Wconversion -Wcast-qual -Wwrite-strings \
	-Wvla -Wformat=2 -Wundef -Wpointer-arith
ifdef WERROR
WARNINGS+=-Werror
endif
LUCY_CFLAGS=$(CSTD) $(WARNINGS)

VERSION=$(shell cat package.json | jq -r '.version')
SRC_FILES=$(shell find src -type f)
CORE_C_FILES=$(shell find src/core -type f -name "*.c")
BIN_C_FILES=$(shell find src/bin -type f -name "*.c")

all: dist/lucy.wasm bin/lucyc
.PHONY: all

# The wasm module is built with clang from wasi-sdk, statically linking
# wasi-libc for malloc and vsnprintf. scripts/get_wasi_sdk downloads the
# pinned release into ~/.cache/wasi-sdk; set WASI_SDK to use another copy.
WASI_SDK ?= $(shell scripts/get_wasi_sdk --dir)
WASM_CC=$(WASI_SDK)/bin/clang --target=wasm32-wasip1 --sysroot=$(WASI_SDK)/share/wasi-sysroot

# The module is a reactor: no main, just the exports below, which
# liblucy.mjs calls. It must import nothing: main-node.mjs and
# main-browser.js instantiate it without any imports (test-wasm checks).
# One build serves development and production.
#
# - -Os: within 1% of -Oz's size and 3% of -O3's speed. -flto makes the
#   module bigger, and wasi-libc's LTO build adds an import (random_get).
# - NDEBUG: an assert that fails would print through stdio.
# - -lc-printscan-long-double: the default printf's code for long doubles
#   (which the compiler never prints) writes an error to stderr. The full
#   version doesn't.
# - The memory grows as malloc needs it, with no maximum.
# - --strip-all drops the names section, which is 8% of the module. A
#   trap's stack trace then shows function numbers instead of names; build
#   with WASM_STRIP= to keep them.
WASM_EXPORTS=malloc free \
	lucy_result_create lucy_result_destroy lucy_result_set_options \
	lucy_compile lucy_result_success lucy_result_js lucy_result_dts \
	lucy_result_diagnostic_count lucy_result_diagnostic \
	lucy_diagnostic_severity lucy_diagnostic_stream lucy_diagnostic_message \
	lucy_diagnostic_line lucy_diagnostic_column lucy_diagnostic_text
WASM_STRIP=-Wl,--strip-all
comma=,
WASM_FLAGS=-Os -DNDEBUG -mexec-model=reactor \
	$(addprefix -Wl$(comma)--export=,$(WASM_EXPORTS)) \
	$(WASM_STRIP) -lc-printscan-long-double

dist/lucy.wasm: $(SRC_FILES)
	@test -x $(WASI_SDK)/bin/clang || { \
		echo "No wasi-sdk at $(WASI_SDK). Run scripts/get_wasi_sdk, or set WASI_SDK." >&2; \
		exit 1; }
	@mkdir -p dist
	$(WASM_CC) $(LUCY_CFLAGS) $(WASM_FLAGS) $(CORE_C_FILES) -o $@

bin/lucyc: $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(LUCY_CFLAGS) $(CFLAGS) ${BIN_C_FILES} $(CORE_C_FILES) -o $@ \
		-DVERSION=\"$(VERSION)\"

# Native builds with AddressSanitizer and UndefinedBehaviorSanitizer, for
# test-sanitize. ASan also turns on leak detection.
SANITIZE_FLAGS=-g -O1 -fno-omit-frame-pointer \
	-fsanitize=address,undefined -fno-sanitize-recover=all
SANITIZE_ENV=ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1

bin/lucyc-sanitize: $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(LUCY_CFLAGS) $(SANITIZE_FLAGS) ${BIN_C_FILES} $(CORE_C_FILES) -o $@ \
		-DVERSION=\"$(VERSION)\"

# Under ASan the arena gives every allocation a block of its own, so that
# overflows between objects are caught. The -arena build keeps the real
# block allocator, so that it runs under the sanitizers too.
bin/compile-all-sanitize: test/sanitize/compile_all.c $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(LUCY_CFLAGS) $(SANITIZE_FLAGS) test/sanitize/compile_all.c $(CORE_C_FILES) -o $@

bin/compile-all-sanitize-arena: test/sanitize/compile_all.c $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(LUCY_CFLAGS) $(SANITIZE_FLAGS) -DARENA_DEBUG=0 test/sanitize/compile_all.c $(CORE_C_FILES) -o $@

clean:
	@rm -rf dist bin
.PHONY: clean

# Format the C sources as .clang-format says. Needs clang-format 18 or later.
format:
	clang-format -i $(shell find src test -name '*.[ch]')
.PHONY: format

# The snapshot tests check the output's text; the behavioural tests
# (test/behavior/) compile each feature, import the module and run it.
test-native:
	@scripts/test_snapshots
	@scripts/test_unit
	@scripts/test_behavior.mjs
.PHONY: test-native

# The same tests through dist/lucy.wasm and liblucy.mjs, plus the
# reuse test, which compiles many inputs in one instance.
test-wasm: dist/lucy.wasm
	@LUCYC=scripts/lucyc.mjs scripts/test_snapshots
	@scripts/test_reuse.mjs
	@LUCYC=scripts/lucyc.mjs scripts/test_behavior.mjs
.PHONY: test-wasm

# The snapshot and unit tests through a sanitizer build of lucyc, then
# every .lucy file under test/, and some generated ones, compiled three
# times over in one process (see test/sanitize/compile_all.c). The
# compiler errors that the error inputs print are only shown on failure.
test-sanitize: bin/lucyc-sanitize bin/compile-all-sanitize bin/compile-all-sanitize-arena
	@$(SANITIZE_ENV) LUCYC=bin/lucyc-sanitize scripts/test_snapshots
	@$(SANITIZE_ENV) LUCYC=bin/lucyc-sanitize scripts/test_unit
	@for harness in bin/compile-all-sanitize bin/compile-all-sanitize-arena; do \
		out=$$($(SANITIZE_ENV) $$harness --rounds 3 --generated \
			$$(find test -name '*.lucy' | sort) 2>&1) || { echo "$$out"; exit 1; }; \
	done
.PHONY: test-sanitize

test: test-native test-wasm
.PHONY: test
