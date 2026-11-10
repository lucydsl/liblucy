// liblucy for Node.js: loads dist/lucy.wasm from disk.
import { readFile } from 'node:fs/promises';
import { createCompiler } from './liblucy.mjs';

export let compile;

export let ready = readFile(new URL('./dist/lucy.wasm', import.meta.url))
  .then(bytes => WebAssembly.instantiate(bytes))
  .then(({ instance }) => {
    compile = createCompiler(instance);
  });
