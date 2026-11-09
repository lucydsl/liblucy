//import createModule from './dist/liblucy-debug.mjs';

export default async function(createModule) {
  const moduleReady = createModule();
  const Module = await moduleReady;

  const {
    lengthBytesUTF8,
    stringToUTF8,
    UTF8ToString
  } = Module;

  const _malloc = Module._malloc;
  const _free = Module._free;
  const _compileXstate = Module._compile_xstate;
  const _xsGetJS = Module._xs_get_js;
  const _xsGetDTS = Module._xs_get_dts;
  const _xsCreate = Module._xs_create;
  const _xsInit = Module._xs_init;
  const _destroyXstateResult = Module._destroy_xstate_result;

  // A NUL-terminated UTF-8 copy of str on the wasm heap, which the caller
  // must _free. These used to go on the stack, which is 64 KB, at four
  // bytes a character, so a source of more than about 16K characters
  // overflowed it and broke the module for every later compile.
  function stringToHeap(str) {
    const size = lengthBytesUTF8(str) + 1;
    const ptr = _malloc(size);
    stringToUTF8(str, ptr, size);
    return ptr;
  }

  /**
   * Compile Lucy source a module of XState machines.
   * @param source {String} the input Lucy source.
   * @param filename {String} the name of the Lucy file.
   * @param options {Object}
   * @returns {String} The compiled JavaScript module.
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
    const fnPtr = stringToHeap(filename);
    const resPtr = _xsCreate();
    _xsInit(resPtr, useRemote, dts);
    _compileXstate(resPtr, srcPtr, fnPtr);
    _free(fnPtr);
    _free(srcPtr);

    // Read the heap only now: it may have grown during the compile.
    const success = !!Module.HEAPU8[resPtr];

    let out = null;
    if(success) {
      out = {};
      out.js = UTF8ToString(_xsGetJS(resPtr));
      if(dts) {
        out.dts = UTF8ToString(_xsGetDTS(resPtr));
      }
    }

    // Destroy the result whether or not the compile succeeded. A failed
    // compile used to skip this and leak it.
    _destroyXstateResult(resPtr);

    if(out === null) {
      throw new Error('Compiler error');
    }
    return out;
  }

  return {
    compileXstate
  };
}
