//import createModule from './dist/liblucy-debug.mjs';

export default async function(createModule) {
  const moduleReady = createModule();
  const Module = await moduleReady;

  const {
    stackAlloc,
    stackRestore,
    stackSave,
    stringToUTF8,
    UTF8ToString
  } = Module;

  const _compileXstate = Module.asm.compile_xstate;
  const _xsGetJS = Module.asm.xs_get_js;
  const _xsCreate = Module.asm.xs_create;
  const _xsInit = Module.asm.xs_init;
  const _destroyXstateResult = Module.asm.destroy_xstate_result;

  function stringToPtr(str) {
    var ret = 0;
    if (str !== null && str !== undefined && str !== 0) { // null string
      // at most 4 bytes per UTF-8 code point, +1 for the trailing '\0'
      var len = (str.length << 2) + 1;
      ret = stackAlloc(len);
      stringToUTF8(str, ret, len);
    }
    return ret;
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
  
    let stack = stackSave();
    let srcPtr = stringToPtr(source);
    let fnPtr = stringToPtr(filename);
    let resPtr = _xsCreate();
    _xsInit(resPtr, options.useRemote);
    _compileXstate(resPtr, srcPtr, fnPtr);
    stackRestore(stack); 
  
    const HEAPU8 = Module.HEAPU8;  
    let success = !!HEAPU8[resPtr];
  
    if(success) {
      let jsPtr = _xsGetJS(resPtr);
      let js = UTF8ToString(jsPtr);
      _destroyXstateResult(resPtr);
      return js;
    }
  
    let err = new Error('Compiler error');
    throw err;
  }

  return {
    compileXstate
  };
}