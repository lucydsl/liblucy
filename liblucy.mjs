import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const Module = require('./dist/liblucy-debug.js');

const _compileXstate = Module.cwrap('compile_xstate', 'string', ['string', 'number']);

export function compileXstate(source) {
  return _compileXstate(source);
}