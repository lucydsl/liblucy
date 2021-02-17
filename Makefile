EMCC=emcc

SRC_FILES=$(shell find src -type f)
CORE_C_FILES=$(shell find src/core -type f -name "*.c")
BIN_C_FILES=$(shell find src/bin -type f -name "*.c")

dist/liblucy-debug.js: $(SRC_FILES)
	@mkdir -p dist
	$(EMCC) src/wasm/main.c $(CORE_C_FILES) -o $@ \
		--pre-js src/pre_js.js \
		--js-transform scripts/transform \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate", "_xs_get_js"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "addOnPostRun", "stringToUTF8", "UTF8ToString"]' \
		-s TEXTDECODER=1
	@rm -f $@.bu

dist/liblucy-release.js: $(SRC_FILES)
	@mkdir -p dist
	$(EMCC) $(CORE_C_FILES) -o $@ \
		--pre-js src/pre_js.js \
		--js-transform scripts/transform \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate", "_xs_get_js"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "addOnPostRun", "stringToUTF8", "UTF8ToString"]' \
		-s TEXTDECODER=1 \
		-O3

bin/lc: $(SRC_FILES)
	@mkdir -p bin
	$(CC) ${BIN_C_FILES} $(CORE_C_FILES) -o $@

clean:
	@rm -f dist/liblucy-debug.js dist/liblucy-debug.wasm dist/liblucy-release.js dist/liblucy-release.wasm
	@rmdir dist
.PHONY: clean

test: bin/lc
	@scripts/test_snapshots $(ARGS)