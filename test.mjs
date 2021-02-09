import { compileXstate as compile } from './liblucy.mjs';
import { performance } from 'perf_hooks';

let before = performance.now();
let ret = compile(`
  import { incrementCount, decrementCount } from './actions.js'

  state enabled {
    toggle => disabled
  }

  initial state disabled {
    toggle => enabled
  }
`);
let after = performance.now();

console.log(ret);
//console.log(after - before);