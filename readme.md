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

- `createMachine({ context, actions, guards, assigns, delays, services })`: the options supply what the Lucy source names with `:symbols`, each called as `(context, event)`; a `services` entry returns a promise for `invoke(:name)`, and for `spawn(:name)` is called with `{ parent }` and returns an unstarted machine. Only the options a machine uses are read.
- A machine that uses `send(parent, event)` sends to whatever created it: the machine that spawned or invoked it, or the `parent` option (anything with a `send`) when you create it yourself. Without a parent the send does nothing. An imported factory given to `spawn` is called with `{ parent }` too. Spawning into a context key that already holds a machine stops that one first.
- `state` is the current state's name, or a dotted path such as `'red.walk'` inside a nested machine. `context` is a plain object, replaced (not mutated) by each `assign`. `done` becomes `true` when a final state is entered.
- `send(event)` takes a type or an `{ type, data }` object and runs the transition at once. An event sent while the machine is handling another (from an action, a listener, or an actor it sent to) is queued: it is handled after that transition and its notification, in order, before the outermost `send` returns. If a transition throws, the queue is dropped and the error reaches the caller. A machine that calls no code of its own has no queue. `subscribe(listener)` calls the listener with the machine after each change and returns an unsubscribe function. `start()` enters the initial state (entry actions, timers, invokes) and returns the machine; `stop()` clears timers, drops pending invokes, stops invoked and spawned children, and clears the listeners.
- Every module comes with a `.d.ts` that types each factory's options and the machine it returns: `lucyc --out-file toggle.js` writes `toggle.d.ts` beside it, and `compile()` returns it as `dts`.
- `lucyc --ts` (and `compile(source, filename, { format: 'ts' })`, which returns `ts`) compiles to one TypeScript module instead: the same code with those types in it. Its syntax is erasable, so Node runs it as it is. The types are those of the factories; the code inside them is marked `// @ts-nocheck`, as it is generated and uses `any` for the events.

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

## Inputs

A machine can declare what it has to be given when it is created, and a machine that spawns it gives those in a block after the spawn:

```lucy
use './types' { Storage }

machine todoItem {
  input title(string)
  input storage(Storage)

  initial state active {
    rename => assign(title) => active
  }
}

machine todoList {
  initial state ready {
    add => assign(todo, spawn(todoItem) {
      title = event.data
      storage = context.storage
    }) => ready
  }
}
```

- An input is a key of the machine's context: the spawned machine starts with `{ title, storage }` as its context, and reads and assigns them like any other key. The type, as for an event, is optional and a single name.
- The block has one `name = value` to a line. A value is a reference, not an expression: `event.data`, `context.key`, `self` (the spawning machine), `parent` (its own parent), or a function, from `use` or a `:symbol` in `assigns`, called with `(context, event)`.
- An imported factory or a `services` entry given to `spawn` is called with `{ context, parent }`, the inputs being the context.
- `send(actor, event)` passes on the data of the event being handled. A third argument, a value of the same kinds, is the data instead: `send(parent, removed, self)` tells the parent which machine was removed, and `send(todo, rename, context.name)` sends a key of the context.
- A line that starts with a `:symbol` gives the machine one of its symbols instead: `:canDelete = :canDeleteTodo` or `:log = log`. The function, one from `use` or one of the spawning machine's own symbols, is handed over as it is, not called, and goes in the option the spawned machine uses it in (`guards.canDelete`), never in its context. A symbol of its own that a machine gives away is one its creator has to supply, in the same option, even if it only passes it on. This works for a machine of the same file; for anything else, the compiler warns.
- The compiler warns about a spawn that leaves out a symbol the machine uses, or gives one it doesn't.
- A machine that is invoked is given both the same way, in the invoke's block beside its transitions: `invoke(countdown) { seconds = event.data` / `:more = :keepGoing` / `done => idle }`. It is a spawned machine that lives as long as the state. What it sends its parent, the state handles as its own events. An invoked function takes neither: it is called with the context and the event.
- The compiler warns about a spawn that leaves out an input the machine declares, or gives one it doesn't, and about an `invoke` of a machine with inputs, which can't give any. A machine that declares no inputs is not checked.
- In the `.d.ts`, the inputs are `TodoItemInput`, and `createTodoItem` has to be given a `context` that holds them.

## Lists of machines

`assign(key, spawn(machine))` keeps one machine. To keep many, `append` adds what a spawn makes to a list, and `remove` takes a machine out of it:

```lucy
machine todoList {
  initial state ready {
    add => append(todos, spawn(todoItem) {
      title = event.data
      list = self
    }) => ready
    removedTodo => remove(todos, event.data) => ready
  }
}
```

- `append(key, spawn(...))` adds the new machine to the list in `context[key]`, which need not exist yet. The list, like the context, is replaced and not changed.
- `remove(key, value)` stops the machine and takes it out of the list, if it is in it; if it isn't, nothing happens. The value is `event.data` or `context.key`.
- `stop()` stops every machine in the list. A machine that reaches a final state stays in the list until it is removed.
- `append` and `remove` are keywords where an action goes, so a state can't be called either and be a transition's target. An event still can.
- The compiler warns about a key that `assign` spawns into and `append` adds to, and about a `send` to a key that holds a list.

## Contributing

See the [contributing page](https://lucy.spooky.click/docs/contributing/) for information on how to build liblucy and the workflow for making changes.

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
