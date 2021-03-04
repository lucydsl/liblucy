const isNodeJS = typeof process === 'object' && Object.prototype.toString.call(process) === '[object process]';
if(isNodeJS) {
  Module.instantiateWasm = function(imports, receiveInstance) {
    var bytes = getBinary(wasmBinaryFile);
    var mod = new WebAssembly.Module(bytes);
    var instance = new WebAssembly.Instance(mod, imports);
  
    receiveInstance(instance, mod);
    return instance.exports;
  };
}

Module.locateFile = function(pth) {
  let url;
  switch(pth) {
    case 'liblucy-debug.wasm': {
      url = new URL('./liblucy-debug.wasm', IMPORT_META_URL);
      break;
    }
    case 'liblucy-release.wasm': {
      url = new URL('./liblucy-release.wasm', IMPORT_META_URL);
      break;
    }
  }
  return url.toString().replace('file://', '');
};