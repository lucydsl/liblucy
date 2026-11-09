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

test: test-native test-wasm test-wasm-release
.PHONY: test
