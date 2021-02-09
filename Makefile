CC=emcc

dist/liblucy-debug.js: $(shell find src -type f)
	@mkdir -p dist
	$(CC) src/main.c -o $@ \
		--pre-js src/instantiate_sync.js \
		-s EXPORTED_FUNCTIONS='["_main", "_compile"]' \
		-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
		-s TEXTDECODER=1

clean:
	@rm -f dist/liblucy-debug.js dist/liblucy-debug.wasm
	@rmdir dist
.PHONY: clean

test:
	node test.mjs
.PHONY: test