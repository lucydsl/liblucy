# liblucy

__liblucy__ is the core piece of [Lucy](https://lucylang.org/) the DSL. It contains the core compiler that is compiled to wasm for usage in JavaScript environments such as Node.js. It also includes the CLI compiler, `lucyc`.

## Contributing

See the [contributing page](https://lucylang.org/docs/contributing/) for information on how to build liblucy and the workflow for making changes.

## License

BSD-2-Clause
## Building

The wasm build needs [Emscripten](https://emscripten.org/) 7.0.0 (the version CI uses). If you don't want to install it, the official container works:

```sh
podman run --rm -v "$PWD":/src -w /src docker.io/emscripten/emsdk:7.0.0 make all
```

The native compiler, `bin/lucyc`, only needs a C compiler: `make bin/lucyc`. Run the tests with `make test`.
