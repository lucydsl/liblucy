import { compileXstate as compile } from './liblucy.mjs';
import { performance } from 'perf_hooks';

let before = performance.now();
let ret = compile(`
  import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js'

  action increment = assign count incrementCount
  action decrement = assign count decrementCount

  guard isNotMax = lessThanTen
  guard isNotMin = greaterThanZero

  initial state active {
    inc => isNotMax => increment
    dec => isNotMin => decrement
  }
`);
let after = performance.now();

console.log(ret);
//console.log(after - before);