// The JavaScript side of dist/lucy.wasm: compile() on top of the C API in
// src/core/lucy.h. main-node.mjs and main-browser.js load the module and
// pass its instance here.
//
// The module imports nothing and exports its memory, malloc and free, and
// the lucy_* functions. Strings cross as NUL-terminated UTF-8 in that
// memory. It grows when malloc needs more, which replaces memory.buffer,
// so views of it are made fresh after every call into the module.

const encoder = new TextEncoder();
const decoder = new TextDecoder();

/**
 * @param instance {WebAssembly.Instance} an instance of dist/lucy.wasm.
 * @param options {{ print?: (text: string) => void, printErr?: (text: string) => void }}
 *   where compile prints diagnostics: lucyc's stdout and stderr, by
 *   default console.log and console.error.
 * @returns the compile function.
 */
export function createCompiler(instance, {
  print = console.log,
  printErr = console.error
} = {}) {
  const wasm = instance.exports;
  // A reactor module's constructors run in _initialize, once, before any
  // other call. lucy.wasm has none today, but that could change.
  wasm._initialize?.();

  // The module's memory as bytes. Make a new view after every call into the
  // module: if the memory grew, the old one is empty.
  function bytes() {
    return new Uint8Array(wasm.memory.buffer);
  }

  // A NUL-terminated UTF-8 copy of str in the module's memory, which the
  // caller must free.
  function writeString(str) {
    const encoded = encoder.encode(str);
    // Pointers come back as signed 32-bit numbers; >>> 0 keeps one above
    // 2 GB positive.
    const ptr = wasm.malloc(encoded.length + 1) >>> 0;
    // malloc returns 0 when the memory can't grow enough. Writing the
    // string there would overwrite the start of memory.
    if(ptr === 0) {
      throw new Error('Out of memory');
    }
    const heap = bytes();
    heap.set(encoded, ptr);
    heap[ptr + encoded.length] = 0;
    return ptr;
  }

  // The NUL-terminated string at ptr. NULL reads as ''.
  function readString(ptr) {
    ptr >>>= 0;
    if(ptr === 0) {
      return '';
    }
    const heap = bytes();
    return decoder.decode(heap.subarray(ptr, heap.indexOf(0, ptr)));
  }

  // The result's errors and warnings, as plain objects.
  function readDiagnostics(result) {
    const diagnostics = [];
    const count = wasm.lucy_result_diagnostic_count(result) >>> 0;
    for(let i = 0; i < count; i++) {
      const diagnostic = wasm.lucy_result_diagnostic(result, i);
      diagnostics.push({
        severity: wasm.lucy_diagnostic_severity(diagnostic) === 0 ? 'error' : 'warning',
        message: readString(wasm.lucy_diagnostic_message(diagnostic)),
        line: wasm.lucy_diagnostic_line(diagnostic),
        column: wasm.lucy_diagnostic_column(diagnostic),
        text: readString(wasm.lucy_diagnostic_text(diagnostic))
      });
    }
    return diagnostics;
  }

  // Print the result's errors and warnings as lucyc does: errors to
  // printErr, and the warnings lucyc prints to stdout to print. The text
  // ends in a newline, which console.log and console.error add themselves.
  function printDiagnostics(result) {
    const count = wasm.lucy_result_diagnostic_count(result) >>> 0;
    for(let i = 0; i < count; i++) {
      const diagnostic = wasm.lucy_result_diagnostic(result, i);
      const text = readString(wasm.lucy_diagnostic_text(diagnostic));
      // 1 is LUCY_STREAM_STDOUT.
      const write = wasm.lucy_diagnostic_stream(diagnostic) === 1 ? print : printErr;
      write(text.endsWith('\n') ? text.slice(0, -1) : text);
    }
  }

  /**
   * Compile Lucy source to a module of plain JavaScript state machines.
   * @param source {String} the input Lucy source.
   * @param filename {String} the name of the Lucy file.
   * @param options {{ dts?: boolean }}
   * @returns {{ js: string, dts?: string }} The compiled JavaScript module,
   *   and its TypeScript declarations if options.dts is set.
   * @throws {Error} 'Compiler error' if the source doesn't compile, with the
   *   errors and warnings in its diagnostics property.
   */
  function compile(source, filename, options = {}) {
    if(!source || !filename) {
      throw new Error('Source and filename are both required.');
    }

    // Read the options before allocating anything, so that bad options
    // can't throw with memory still allocated.
    const dts = options.dts;

    const srcPtr = writeString(source);
    let fnPtr;
    try {
      fnPtr = writeString(filename);
    } catch(err) {
      wasm.free(srcPtr);
      throw err;
    }
    let result = 0;
    let out = null;
    let diagnostics = null;
    try {
      result = wasm.lucy_result_create();
      // dts goes through ToInt32, as it always has: true is 1, and
      // anything that isn't a number is 0.
      wasm.lucy_result_set_options(result, dts);
      wasm.lucy_compile(result, srcPtr, fnPtr);

      // The compiler doesn't print; this prints what lucyc would.
      printDiagnostics(result);

      if(wasm.lucy_result_success(result)) {
        out = {};
        out.js = readString(wasm.lucy_result_js(result));
        if(dts) {
          out.dts = readString(wasm.lucy_result_dts(result));
        }
      } else {
        diagnostics = readDiagnostics(result);
      }
    } finally {
      // Free everything, even if the module trapped. Destroying NULL does
      // nothing.
      wasm.lucy_result_destroy(result);
      wasm.free(fnPtr);
      wasm.free(srcPtr);
    }

    if(out === null) {
      const err = new Error('Compiler error');
      err.diagnostics = diagnostics;
      throw err;
    }
    return out;
  }

  return compile;
}
