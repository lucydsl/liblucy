#!/usr/bin/env node
// Compiles every snapshot twice in one module instance. Each snapshot test
// runs in a fresh process, so this is what catches state that leaks or is
// freed wrongly between compiles. Between the two rounds it compiles inputs
// that used to break the instance for every compile after them.
import { readdirSync, readFileSync, existsSync } from 'fs';
import init from '../liblucy.mjs';

const env = process.env.NODE_ENV || 'development';
const build = env === 'production' ? 'release' : 'debug';
const { default: createModule } = await import(`../dist/liblucy-${build}-node.mjs`);

// Failing compiles print their errors. Keep the expected ones out of the
// test output.
let quiet = false;
const { compileXstate } = await init(() => createModule({
  printErr(message) {
    if(!quiet) console.error(message);
  }
}));

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
      const result = compileXstate(readFileSync(filename, 'utf-8'), filename, { dts });
      actual = (dts ? result.dts : result.js) + '\n';
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
  const result = compileXstate(`state ${longName} {\n  go => b\n}\n`, 'long.lucy');
  if(!result.js.includes(`\n      ${longName}: {\n`)) {
    fail('long identifier: wrong output');
  }
} catch(err) {
  fail(`long identifier: ${err}`);
}

// Failed compiles used to leak their result.
quiet = true;
for(let i = 0; i < 50; i++) {
  for(const name of errorTests) {
    const filename = `${dir}/${name}/input.lucy`;
    try {
      compileXstate(readFileSync(filename, 'utf-8'), filename);
      fail(`${filename} compiled`);
    } catch(err) {
      if(err.message !== 'Compiler error') {
        fail(`${filename}: ${err}`);
      }
    }
  }
}
quiet = false;

compileSnapshots(2);

process.exit(failed ? 1 : 0);
