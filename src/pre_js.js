let _require;

if(typeof process === 'object' && Object.prototype.toString.call(process) === '[object process]') {
  _require = require;
  Module.instantiateWasm = function(imports, receiveInstance) {
    var bytes = getBinary(wasmBinaryFile);
    var mod = new WebAssembly.Module(bytes);
    var instance = new WebAssembly.Instance(mod, imports);
  
    receiveInstance(instance, mod);
    return instance.exports;
  };
} else {
  Module.locateFile = function(pth) {
    return new URL(pth, location.href).toString();
  };
}