EMCC=emcc

VERSION=$(shell cat package.json | jq -r '.version')
SRC_FILES=$(shell find src -type f)
CORE_C_FILES=$(shell find src/core -type f -name "*.c")
BIN_C_FILES=$(shell find src/bin -type f -name "*.c")
WASM_C_FILES=$(shell find src/wasm -type f -name "*.c")

all: dist/liblucy-debug-node.mjs dist/liblucy-debug-browser.mjs \
	dist/liblucy-release-node.mjs dist/liblucy-release-browser.mjs bin/lucyc
.PHONY: all

dist:
	@mkdir -p dist

EMCC_FLAGS=$(WASM_C_FILES) $(CORE_C_FILES) \
	-sEXPORT_ES6 \
	-sMODULARIZE \
	-sALLOW_MEMORY_GROWTH \
	-sEXPORTED_FUNCTIONS=_main,_compile_xstate,_xs_get_js,_xs_get_dts,_xs_init,_xs_create,_destroy_xstate_result,_malloc,_free \
	-sEXPORTED_RUNTIME_METHODS=stringToUTF8,UTF8ToString,lengthBytesUTF8,HEAPU8

dist/liblucy-debug-node.mjs: $(SRC_FILES) | dist
	$(EMCC) $(EMCC_FLAGS) -sENVIRONMENT=node -o $@

dist/liblucy-debug-browser.mjs: $(SRC_FILES) | dist
	$(EMCC) $(EMCC_FLAGS) -sENVIRONMENT=web -o $@

dist/liblucy-release-node.mjs: $(SRC_FILES) | dist
	$(EMCC) $(EMCC_FLAGS) -sENVIRONMENT=node -O3 -o $@

dist/liblucy-release-browser.mjs: $(SRC_FILES) | dist
	$(EMCC) $(EMCC_FLAGS) -sENVIRONMENT=web -O3 -o $@

bin/lucyc: $(SRC_FILES)
	@mkdir -p bin
	$(CC) ${BIN_C_FILES} $(CORE_C_FILES) -o $@ \
		-DVERSION=\"$(VERSION)\" \
		-lm

# Native builds with AddressSanitizer and UndefinedBehaviorSanitizer, for
# test-sanitize. ASan also turns on leak detection.
SANITIZE_FLAGS=-g -O1 -fno-omit-frame-pointer \
	-fsanitize=address,undefined -fno-sanitize-recover=all
SANITIZE_ENV=ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1

bin/lucyc-sanitize: $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(SANITIZE_FLAGS) ${BIN_C_FILES} $(CORE_C_FILES) -o $@ \
		-DVERSION=\"$(VERSION)\" \
		-lm

# Under ASan the arena gives every allocation a block of its own, so that
# overflows between objects are caught. The -arena build keeps the real
# block allocator, so that it runs under the sanitizers too.
bin/compile-all-sanitize: test/sanitize/compile_all.c $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(SANITIZE_FLAGS) test/sanitize/compile_all.c $(CORE_C_FILES) -o $@ -lm

bin/compile-all-sanitize-arena: test/sanitize/compile_all.c $(SRC_FILES)
	@mkdir -p bin
	$(CC) $(SANITIZE_FLAGS) -DARENA_DEBUG=0 test/sanitize/compile_all.c $(CORE_C_FILES) -o $@ -lm

clean:
	@rm -rf dist bin
.PHONY: clean

test-native:
	@scripts/test_snapshots
	@scripts/test_unit
.PHONY: test-native

test-wasm:
	@LUCYC=scripts/lucyc.mjs scripts/test_snapshots
	@scripts/test_reuse.mjs
.PHONY: test-wasm

test-wasm-release:
	@LUCYC=scripts/lucyc.mjs NODE_ENV=production scripts/test_snapshots
	@NODE_ENV=production scripts/test_reuse.mjs
.PHONY: test-wasm-release

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

test: test-native test-wasm test-wasm-release
.PHONY: test
