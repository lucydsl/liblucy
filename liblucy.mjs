import Module from './dist/liblucy-debug.js';

const _compileXstate = Module.cwrap('compile_xstate', 'string', ['string', 'number']);

export function compileXstate(source) {
  return _compileXstate(source);
}

export let init;
if(Module.calledRun) {
  init = Promise.resolve();
} else {
  Module.addOnPostRun(() => init = Promise.resolve());
}