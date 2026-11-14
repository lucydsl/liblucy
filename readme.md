# liblucy

__liblucy__ is the core piece of [Lucy](https://lucy.spooky.click/) the DSL. It contains the core compiler that is compiled to wasm for usage in JavaScript environments such as Node.js. It also includes the CLI compiler, `lucyc`.

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

## Typed events

A machine can declare its events, and the type of the data each carries:

```lucy
use './types' { User }

machine session {
  event login(User)
  event rename(string)
  event logout

  initial state anonymous {
    login => assign(user) => active
  }

  state active {
    rename => assign(name)
    logout => anonymous
  }
}
```

- A type is a single name: one imported with `use`, or one TypeScript already knows (`string`, `number`, `MouseEvent`). Give any other type (`string[]`, a union) a name in a `.ts` file and import it. A name used as an event's type is imported as a type only: the JavaScript leaves it out and the `.d.ts` has `import type`.
- The type is that of the event's `data`: `machine.send({ type: 'login', data: user })`.
- Events are declared in a top-level `machine { }` block, for that machine and the machines nested in its states. An event has one type, whichever state handles it.
- The declarations change nothing in the JavaScript. The compiler warns about a transition on an event the machine doesn't declare, a declared event no state handles, `assign(key)` on an event declared without data, and `send(child, event)` to a machine spawned from the same file that doesn't declare `event`. A machine that declares no events is not checked.
- In the `.d.ts`, the machine's events become `SessionEvents` (by name, so `SessionEvents['login']` types the event parameter of an imported guard or action) and their union `SessionEvent`. `send` takes a `SessionEvent`, or the name alone for an event without data. What the options supply is typed with the events it is used on, and the `context` passed in has to be able to hold the data that `assign(key)` stores.

## Contributing

See the [contributing page](https://lucy.spooky.click/docs/contributing/) for information on how to build liblucy and the workflow for making changes.

## License

BSD-2-Clause
## Building

The wasm build needs [Emscripten](https://emscripten.org/) 7.0.0 (the version CI uses). If you don't want to install it, the official container works:

```sh
podman run --rm -v "$PWD":/src -w /src docker.io/emscripten/emsdk:7.0.0 make all
```

The native compiler, `bin/lucyc`, only needs a C17 compiler: `make bin/lucyc`. Run the tests with `make test`. The code builds without warnings under the strict set in the Makefile; `make WERROR=1 ...` makes them errors, as CI does.

`src/README.md` describes how the sources are laid out and the conventions they follow.

`make test-sanitize` checks for memory errors and leaks: it runs the tests, and every `.lucy` file under `test/`, through native builds with AddressSanitizer and UndefinedBehaviorSanitizer. It needs GCC or Clang on Linux.
