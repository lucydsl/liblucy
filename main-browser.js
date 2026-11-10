// liblucy for browsers (and bundlers, which recognise the new URL(...,
// import.meta.url) below): fetches dist/lucy.wasm next to this file.
import { createCompiler } from './liblucy.mjs';

const url = new URL('./dist/lucy.wasm', import.meta.url);

async function instantiate() {
  const response = await fetch(url);
  if(!response.ok) {
    throw new Error(`Couldn't fetch ${url}: ${response.status}`);
  }
  // Streaming compiles while the bytes download, but only works when the
  // server sends the file as application/wasm.
  if(WebAssembly.instantiateStreaming &&
     response.headers.get('Content-Type') === 'application/wasm') {
    return (await WebAssembly.instantiateStreaming(response)).instance;
  }
  return (await WebAssembly.instantiate(await response.arrayBuffer())).instance;
}

export let compile;

export let ready = instantiate().then(instance => {
  compile = createCompiler(instance);
});
