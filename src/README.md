# liblucy's C sources

## Layout

- `bin/main.c`: `lucyc`, the command-line compiler.
- `wasm/main.c`: the wasm module's own entry points: `main`, and the
  function that prints a compile's diagnostics for `liblucy.mjs`.
- `core/`: the compiler itself, built into both.
  - `lucy.h`: the library's API, which `lucyc` and the wasm module call.
    `lucy.c` implements it: it parses the source and prints the result.
  - `diagnostic.h`: the list of errors and warnings a compile reports, and
    how an error's text (with the source around it) is laid out.
  - `support/`: general-purpose code that knows nothing about Lucy: the
    arena allocator, checked `malloc` (`mem.h`), a string builder, and a
    string set, map and list.
  - `ast/`: the syntax tree. Nodes (`Node`, `MachineNode`, `StateNode`,
    ...) and expressions (`Expression`, `IdentifierExpression`, ...).
  - `parser/`: turns source into a `Program`. `parser.h` is all the rest of
    the compiler sees; `internal.h` is shared by the parser's own files.
    `lexer.c` holds the cursor and the tokens, and the other files each
    parse one construct (`machine.c`, `state.c`, `transition.c`, ...).
  - `xstate/`: prints a `Program` as XState JavaScript. `xstate.h` is its
    interface; `printer.h` is shared by the files that print each construct.
    `ts_printer.c` prints the TypeScript declarations, and `js_builder.c`
    builds indented JavaScript text.

## Conventions

The code is C17 (`-std=c17`), built with the strict set of warnings in the
Makefile. The core uses only ISO C, so the same sources build natively and
with Emscripten. `lucyc` also uses POSIX.

### Naming

- **Types** are `PascalCase`: `Parser`, `StrBuilder`, `MachineNode`.
  Acronyms stay in capitals (`JSBuilder`, `TSPrinter`). No `_t` suffix:
  POSIX reserves it. Structs are typedef'd to the same name
  (`typedef struct Parser { ... } Parser;`).
- **Functions** are `snake_case`, prefixed with their module. That is the
  type they work on, which is their first parameter:
  `str_builder_add_str(StrBuilder*, ...)`, `parser_next_token(Parser*)`,
  `printer_enter_state(Printer*, ...)`, `node_append_child(Node*, ...)`.
  Functions without such a parameter take the name of their file instead:
  `keyword_lookup` in `keyword.c`, `timeframe_parse` in `timeframe.c`,
  `expression_create_symbol` in `expression.c`. `static` functions need no
  prefix.
  - `_create` makes an object. If it comes from the heap there is a
    matching `_destroy`. If it comes from an arena (and takes the `Arena*`)
    there isn't: it goes when the arena does.
  - `_init` sets up an object the caller already has, such as one on the
    stack.
  - The exceptions are the allocators in `support/mem.h`, which follow the
    common `xmalloc` idiom, and `size_add` and `size_mul` there.
- **The public API** in `lucy.h` starts every name with `lucy_` (types with
  `Lucy`, constants with `LUCY_`), then follows the same scheme:
  `lucy_result_js` works on a `LucyResult`.
- **Enums** are typedef'd `PascalCase` types whose constants are
  `UPPER_SNAKE_CASE` and start with the type's name: `NodeType` has
  `NODE_MACHINE` and `NODE_STATE`, `TokenType` has `TOKEN_EOF`, `Keyword`
  has `KEYWORD_USE`. Use them for any set of codes, never `#define`d
  integers.
- **Flags** are `#define`d unsigned bits, parenthesised, named after what
  holds them: `#define MACHINE_USES_GUARD (1u << 0)` for
  `MachineNode.flags`. Flag fields are `unsigned`.
- **Variables, fields and parameters** are `snake_case`. A `Parser*` is
  `parser`, a `Printer*` is `printer`, a `JSBuilder*` is `jsb`.
- **Files** are `snake_case`, named after the type or module they hold.

### Headers

- Every header uses `#pragma once`.
- Every `.c` file includes its own header first, so the compiler checks the
  header against the definitions, then system headers, then the project's,
  each group in order. A module whose files share one header (`parser/`,
  `xstate/printer.h`) counts that as each file's own header.
- Functions used in only one file are `static`. Every function has a
  prototype, and one that takes no arguments says `(void)`.

### Memory

Each compile allocates its syntax tree and working data from one arena
(`support/arena.h`), freed all at once at the end, so nothing in the tree is
freed on its own. Memory that outlives a compile (the output strings) comes
from `malloc` through `support/mem.h`, whose functions abort on failure
rather than return NULL. Each header says who owns what it returns.

### Errors and diagnostics

- The library never prints. The parser and the printer add errors and
  warnings to the result's diagnostics (`diagnostic.h`), and `lucyc` and
  `wasm/main.c` print them. The one exception is the allocators in
  `support/mem.h`, which print a line before they abort.
- Parse functions return a `ParseStatus`: `PARSE_OK`, `PARSE_ERROR` (an
  error was reported, but parsing can go on and find more) or `PARSE_FATAL`
  (stop now). Combine the status of a nested parse with `parse_merge`, and
  return at once when it is fatal; see `parser/internal.h`.
- A behaviour that is wrong but kept, because fixing it would change what
  the compiler outputs, is marked with a `// Known bug:` comment.

