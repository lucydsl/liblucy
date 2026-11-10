#!/usr/bin/env node
// Compiles every snapshot twice in one module instance. Each snapshot test
// runs in a fresh process, so this is what catches state that leaks or is
// freed wrongly between compiles. Between the two rounds it compiles inputs
// that used to break the instance for every compile after them.
import { readdirSync, readFileSync, existsSync } from 'fs';
import { createCompiler } from '../liblucy.mjs';

// The module has to work with no imports at all, which a libc function
// that calls into WASI would break.
const wasmModule = new WebAssembly.Module(readFileSync('dist/lucy.wasm'));
const imports = WebAssembly.Module.imports(wasmModule);
if(imports.length) {
  console.log(`FAILED - dist/lucy.wasm imports ${imports.map(i => `${i.module}.${i.name}`).join(', ')}`);
  process.exit(1);
}
const instance = new WebAssembly.Instance(wasmModule, {});

// Failing compiles print their errors. Keep the expected ones out of the
// test output.
let quiet = false;
const compile = createCompiler(instance, {
  printErr(message) {
    if(!quiet) console.error(message);
  }
});

const dir = 'test/snapshots';
const names = readdirSync(dir).filter(name => !existsSync(`${dir}/${name}/.skip`));
const tests = names.filter(name => !name.startsWith('error_'));
const errorTests = names.filter(name => name.startsWith('error_'));
let failed = 0;

function fail(message) {
  console.log(`FAILED - ${message}`);
  failed++;
}

function compileSnapshots(round) {
  for(const name of tests) {
    const filename = `${dir}/${name}/input.lucy`;
    const dts = name.startsWith('dts_');
    const expectedFile = `${dir}/${name}/expected.${dts ? 'd.ts' : 'js'}`;
    const expected = readFileSync(expectedFile, 'utf-8');
    let actual;
    try {
      const result = compile(readFileSync(filename, 'utf-8'), filename, { dts });
      actual = dts ? result.dts : result.js;
    } catch(err) {
      actual = String(err);
    }
    if(actual !== expected) {
      fail(`(round ${round}) ${filename}`);
    }
  }
}

compileSnapshots(1);

// The source used to be copied onto the 64 KB stack, which this
// overflowed, and the heap was a fixed 16 MB, which this outgrew.
const longName = 'a'.repeat(5000000);
try {
  const result = compile(`state ${longName} {\n  go => ${longName}\n}\n`, 'long.lucy');
  if(!result.js.includes(`\n    ${longName}: {\n`)) {
    fail('long identifier: wrong output');
  }
} catch(err) {
  fail(`long identifier: ${err}`);
}

// The parser recursed without limit, and about 150 nested machines
// overflowed the stack in the debug build. 64 levels compile, even with an
// error reported from the deepest; more are an error.
function nest(depth, inner = '') {
  return 'machine m {\n  state a {\n'.repeat(depth) + inner + '}\n}\n'.repeat(depth);
}
quiet = true;
for(const [depth, inner, ok] of [[64, '', true], [64, '  bogus\n', false], [65, '', false], [5000, '', false]]) {
  try {
    compile(nest(depth, inner), 'nested.lucy');
    if(!ok) fail(`${depth} nested machines compiled`);
  } catch(err) {
    if(ok || err.message !== 'Compiler error') {
      fail(`${depth} nested machines: ${err}`);
    }
  }
}

// Failed compiles used to leak their result. The error they throw carries
// the diagnostics, whose text is what lucyc prints before its own
// "Compilation failed!".
for(let i = 0; i < 50; i++) {
  for(const name of errorTests) {
    const filename = `${dir}/${name}/input.lucy`;
    try {
      compile(readFileSync(filename, 'utf-8'), filename);
      fail(`${filename} compiled`);
    } catch(err) {
      if(err.message !== 'Compiler error') {
        fail(`${filename}: ${err}`);
        continue;
      }
      const expected = readFileSync(`${dir}/${name}/expected.error`, 'utf-8');
      const text = err.diagnostics.map(d => d.text).join('') + 'Compilation failed!\n';
      if(text !== expected || !err.diagnostics.every(d => d.severity === 'error' && d.line > 0)) {
        fail(`${filename}: diagnostics don't match expected.error`);
      }
    }
  }
}
quiet = false;

compileSnapshots(2);

process.exit(failed ? 1 : 0);
