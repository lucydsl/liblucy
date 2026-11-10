# liblucy

__liblucy__ is the core piece of [Lucy](https://lucylang.org/) the DSL. It contains the core compiler that is compiled to wasm for usage in JavaScript environments such as Node.js. It also includes the CLI compiler, `lucyc`.

## Output

The compiler turns a Lucy file into a module of plain JavaScript state machines, with no runtime library: one exported factory per machine (`createLight` for `machine light`, a default export for the file's unnamed machine), each a small set of ordinary functions for just what that machine does.

```js
import createMachine from './toggle.js';

const machine = createMachine({ context: { count: 0 } });
machine.subscribe(m => render(m.state, m.context));
machine.start();
machine.send('toggle');
machine.send({ type: 'set', data: 5 });
console.log(machine.state, machine.context);
machine.stop();
```

- `createMachine({ context, actions, guards, assigns, delays, services })`: the options supply what the Lucy source names with `:symbols`, each called as `(context, event)`; a `services` entry returns a promise for `invoke(:name)` and an unstarted machine for `spawn(:name)`. Only the options a machine uses are read.
- `state` is the current state's name, or a dotted path such as `'red.walk'` inside a nested machine. `context` is a plain object, replaced (not mutated) by each `assign`. `done` becomes `true` when a final state is entered.
- `send(event)` takes a type or an `{ type, data }` object and runs the transition at once. `subscribe(listener)` calls the listener with the machine after each change and returns an unsubscribe function. `start()` enters the initial state (entry actions, timers, invokes) and returns the machine; `stop()` clears timers, drops pending invokes, stops invoked and spawned children, and clears the listeners.
- `--experimental-dts` writes a `.d.ts` that types each factory's options and the machine it returns.

## Contributing

See the [contributing page](https://lucylang.org/docs/contributing/) for information on how to build liblucy and the workflow for making changes.

## License

BSD-2-Clause
## Building

The wasm module, `dist/lucy.wasm`, is built with clang from [wasi-sdk](https://github.com/WebAssembly/wasi-sdk), statically linking wasi-libc. It imports nothing, and `liblucy.mjs` drives it through the standard WebAssembly API, with `main-node.mjs` and `main-browser.js` loading it. `scripts/get_wasi_sdk` downloads the pinned wasi-sdk release into `~/.cache/wasi-sdk`, where the Makefile looks for it (set `WASI_SDK` to use another copy):

```sh
scripts/get_wasi_sdk
make all
```

The native compiler, `bin/lucyc`, only needs a C17 compiler: `make bin/lucyc`. Run the tests with `make test`. The code builds without warnings under the strict set in the Makefile; `make WERROR=1 ...` makes them errors, as CI does.

`src/README.md` describes how the sources are laid out and the conventions they follow.

`make test-sanitize` checks for memory errors and leaks: it runs the tests, and every `.lucy` file under `test/`, through native builds with AddressSanitizer and UndefinedBehaviorSanitizer. It needs GCC or Clang on Linux.
