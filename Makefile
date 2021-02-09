CC=emcc

SRC_FILES=$(shell find src -type f)
C_FILES=$(shell find src -type f -name "*.c")

dist/liblucy-debug.js: $(SRC_FILES)
	@mkdir -p dist
	$(CC) $(C_FILES) -o $@ \
		--pre-js src/instantiate_sync.js \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
		-s TEXTDECODER=1

dist/liblucy-release.js: $(SRC_FILES)
	@mkdir -p dist
	$(CC) $(C_FILES) -o $@ \
		--pre-js src/instantiate_sync.js \
		-s EXPORTED_FUNCTIONS='["_main", "_compile_xstate"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
		-s TEXTDECODER=1 \
		-O3

clean:
	@rm -f dist/liblucy-debug.js dist/liblucy-debug.wasm dist/liblucy-release.js dist/liblucy-release.wasm
	@rmdir dist
.PHONY: clean

test:
	node test.mjs
.PHONY: test