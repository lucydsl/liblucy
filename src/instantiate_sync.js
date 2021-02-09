Module.instantiateWasm = function(imports, receiveInstance) {
  var bytes = getBinary(wasmBinaryFile);
  var mod = new WebAssembly.Module(bytes);
  var instance = new WebAssembly.Instance(mod, imports);

  receiveInstance(instance, mod);
  return instance.exports;
};