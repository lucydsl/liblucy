EMCC=emcc

VERSION=$(shell cat package.json | jq -r '.version')
SRC_FILES=$(shell find src -type f)
CORE_C_FILES=$(shell find src/core -type f -name "*.c")
BIN_C_FILES=$(shell find src/bin -type f -name "*.c")
WASM_C_FILES=$(shell find src/wasm -type f -name "*.c")

all: dist/liblucy-debug-node.mjs dist/liblucy-debug-browser.mjs \
	dist/liblucy-release-node.mjs dist/liblucy-release-browser.mjs bin/lucyc
.PHONY: all

build:
	@mkdir -p build

dist:
	@mkdir -p dist

build/liblucy-debug.mjs: build $(SRC_FILES)
	$(EMCC) $(WASM_C_FILES) $(CORE_C_FILES) -o $@ \
		--pre-js src/pre_js.js \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate", "_xs_get_js", "_xs_init", "_xs_create", "_destroy_xstate_result"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "addOnPostRun", "stringToUTF8", "UTF8ToString"]' \
		-s EXPORT_ES6 \
		-s TEXTDECODER=1
	scripts/post_compile_mjs $@

build/liblucy-release.mjs: build $(SRC_FILES)
	$(EMCC) $(WASM_C_FILES) $(CORE_C_FILES) -o $@ \
		--pre-js src/pre_js.js \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate", "_xs_get_js", "_xs_init", "_xs_create", "_destroy_xstate_result"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "addOnPostRun", "stringToUTF8", "UTF8ToString"]' \
		-s TEXTDECODER=1 \
		-O3
	scripts/post_compile_mjs $@

dist/liblucy-debug.wasm: dist build/liblucy-debug.mjs
	@mv build/liblucy-debug.wasm $@

dist/liblucy-release.wasm: dist build/liblucy-release.mjs
	@mv build/liblucy-release.wasm $@

dist/liblucy-debug-node.mjs: dist build/liblucy-debug.mjs dist/liblucy-debug.wasm
	scripts/mk_node_mjs build/liblucy-debug.mjs $@

dist/liblucy-debug-browser.mjs: dist build/liblucy-debug.mjs dist/liblucy-debug.wasm
	cp build/liblucy-debug.mjs $@

dist/liblucy-release-node.mjs: dist build/liblucy-release.mjs dist/liblucy-release.wasm
	scripts/mk_node_mjs build/liblucy-release.mjs $@

dist/liblucy-release-browser.mjs: dist build/liblucy-release.mjs dist/liblucy-release.wasm
	cp build/liblucy-release.mjs $@

bin/lucyc: $(SRC_FILES)
	@mkdir -p bin
	$(CC) ${BIN_C_FILES} $(CORE_C_FILES) -o $@ \
		-DVERSION=\"$(VERSION)\" \
		-lm

clean:
	@rm -f dist/liblucy-debug-browser.mjs dist/liblucy-debug-node.mjs \
		dist/liblucy-debug.wasm dist/liblucy-release-browser.mjs \
		dist/liblucy-release-node.mjs dist/liblucy-release.wasm
	@rm -f bin/lucyc
	@rm -f build/liblucy-debug.mjs build/liblucy-release.mjs
	@rmdir dist bin build 2> /dev/null
.PHONY: clean

test-native:
	@scripts/test_snapshots
	@scripts/test_unit
.PHONY: test-native

test-wasm:
	@LUCYC=scripts/lucyc.mjs scripts/test_snapshots
.PHONY: test-wasm

test-wasm-release:
	@LUCYC=scripts/lucyc.mjs NODE_ENV=production scripts/test_snapshots
.PHONY: test-wasm-release

test: test-native test-wasm test-wasm-release
.PHONY: test
