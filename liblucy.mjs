import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const Module = require('./dist/liblucy-debug.js');

const _compile = Module.cwrap('compile', 'string', ['string', 'number']);

export function compile(source) {
  return _compile(source);
}