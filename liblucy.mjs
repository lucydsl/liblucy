import Module from './dist/liblucy-debug.js';

const {
  stackAlloc,
  stackRestore,
  stackSave,
  stringToUTF8,
  UTF8ToString
} = Module;

const _compileXstate = Module.asm.compile_xstate;
const _xs_get_js = Module.asm.xs_get_js;

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

export function compileXstate(source) {
  let stack = stackSave();
  let ptr = stringToPtr(source);
  let retPtr = _compileXstate(ptr);
  stackRestore(stack); 

  const HEAPU8 = Module.HEAPU8;  
  let success = !!HEAPU8[retPtr];

  if(success) {
    let jsPtr = _xs_get_js(retPtr);
    let js = UTF8ToString(jsPtr);
    return js;
  }

  let err = new Error('Compiler error');
  throw err;
}

export let init;
if(Module.calledRun) {
  init = Promise.resolve();
} else {
  init = new Promise(resolve => {
    Module.addOnPostRun(() => resolve());
  });
}