#!/usr/bin/env node
// Compiles every snapshot twice in one module instance. Each snapshot test
// runs in a fresh process, so this is what catches state that leaks or is
// freed wrongly between compiles.
import { readdirSync, readFileSync, existsSync } from 'fs';

const env = process.env.NODE_ENV || 'development';
const rel = env === 'production' ? 'prod' : 'dev';
const liblucy = await import(`../main-node-${rel}.mjs`);
await liblucy.ready;
const { compileXstate } = liblucy;

const dir = 'test/snapshots';
const tests = readdirSync(dir)
  .filter(name => !name.startsWith('error_'))
  .filter(name => !existsSync(`${dir}/${name}/.skip`));
let failed = 0;

for(let round = 1; round <= 2; round++) {
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
      console.log(`FAILED (round ${round}) - ${filename}`);
      failed++;
    }
  }
}

process.exit(failed ? 1 : 0);
