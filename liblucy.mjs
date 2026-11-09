//import createModule from './dist/liblucy-debug.mjs';

export default async function(createModule) {
  const moduleReady = createModule();
  const Module = await moduleReady;

  const {
    lengthBytesUTF8,
    stringToUTF8,
    UTF8ToString
  } = Module;

  // The C API, from src/core/lucy.h.
  const _malloc = Module._malloc;
  const _free = Module._free;
  const _resultCreate = Module._lucy_result_create;
  const _resultDestroy = Module._lucy_result_destroy;
  const _resultSetOptions = Module._lucy_result_set_options;
  const _compileXstate = Module._lucy_compile_xstate;
  const _resultSuccess = Module._lucy_result_success;
  const _resultJS = Module._lucy_result_js;
  const _resultDTS = Module._lucy_result_dts;
  const _diagnosticCount = Module._lucy_result_diagnostic_count;
  const _diagnostic = Module._lucy_result_diagnostic;
  const _diagnosticSeverity = Module._lucy_diagnostic_severity;
  const _diagnosticMessage = Module._lucy_diagnostic_message;
  const _diagnosticLine = Module._lucy_diagnostic_line;
  const _diagnosticColumn = Module._lucy_diagnostic_column;
  const _diagnosticText = Module._lucy_diagnostic_text;
  // From src/wasm/main.c.
  const _printDiagnostics = Module._wasm_print_diagnostics;

  // A NUL-terminated UTF-8 copy of str on the wasm heap, which the caller
  // must _free. These used to go on the stack, which is 64 KB, at four
  // bytes a character, so a source of more than about 16K characters
  // overflowed it and broke the module for every later compile.
  function stringToHeap(str) {
    const size = lengthBytesUTF8(str) + 1;
    const ptr = _malloc(size);
    // The heap can grow, and when it can't grow enough malloc returns 0
    // rather than aborting. Writing the string there would overwrite the
    // start of memory.
    if(ptr === 0) {
      throw new Error('Out of memory');
    }
    stringToUTF8(str, ptr, size);
    return ptr;
  }

  // The result's errors and warnings, as plain objects.
  function readDiagnostics(resPtr) {
    const diagnostics = [];
    const count = _diagnosticCount(resPtr);
    for(let i = 0; i < count; i++) {
      const ptr = _diagnostic(resPtr, i);
      diagnostics.push({
        severity: _diagnosticSeverity(ptr) === 0 ? 'error' : 'warning',
        message: UTF8ToString(_diagnosticMessage(ptr)),
        line: _diagnosticLine(ptr),
        column: _diagnosticColumn(ptr),
        text: UTF8ToString(_diagnosticText(ptr))
      });
    }
    return diagnostics;
  }

  /**
   * Compile Lucy source to a module of XState machines.
   * @param source {String} the input Lucy source.
   * @param filename {String} the name of the Lucy file.
   * @param options {{ useRemote?: boolean, dts?: boolean }}
   * @returns {{ js: string, dts?: string }} The compiled JavaScript module,
   *   and its TypeScript declarations if options.dts is set.
   * @throws {Error} 'Compiler error' if the source doesn't compile, with the
   *   errors and warnings in its diagnostics property.
   */
  function compileXstate(source, filename, options = {
    useRemote: false
  }) {
    if(!source || !filename) {
      throw new Error('Source and filename are both required.');
    }

    // Read the options before allocating anything, so that bad options
    // can't throw with memory still allocated.
    const useRemote = options.useRemote;
    const dts = options.dts;

    const srcPtr = stringToHeap(source);
    let fnPtr;
    try {
      fnPtr = stringToHeap(filename);
    } catch(err) {
      _free(srcPtr);
      throw err;
    }
    const resPtr = _resultCreate();
    _resultSetOptions(resPtr, useRemote, dts);
    _compileXstate(resPtr, srcPtr, fnPtr);
    _free(fnPtr);
    _free(srcPtr);

    // The compiler doesn't print. This prints its errors and warnings to
    // stdout and stderr, which reach Module.print and Module.printErr just
    // as they did when the compiler printed them itself.
    _printDiagnostics(resPtr);

    let out = null;
    let diagnostics = null;
    if(_resultSuccess(resPtr)) {
      out = {};
      out.js = UTF8ToString(_resultJS(resPtr));
      if(dts) {
        out.dts = UTF8ToString(_resultDTS(resPtr));
      }
    } else {
      diagnostics = readDiagnostics(resPtr);
    }

    // Destroy the result whether or not the compile succeeded. A failed
    // compile used to skip this and leak it.
    _resultDestroy(resPtr);

    if(out === null) {
      const err = new Error('Compiler error');
      err.diagnostics = diagnostics;
      throw err;
    }
    return out;
  }

  return {
    compileXstate
  };
}
