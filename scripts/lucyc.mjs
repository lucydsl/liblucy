#!/usr/bin/env node
import { promises as fsPromises } from 'fs';
const { readFile } = fsPromises;

const [,, filename] = process.argv;

if(!filename) {
  console.error('A filename is required')
  process.exit(1);
}

async function run() {
  const [
    contents,
    { compileXstate, ready }
  ] = await Promise.all([
    readFile(filename, 'utf-8'),
    import('../main-node-dev.mjs') // dynamic to support debug/release mode
  ]);
  await ready;

  try {
    const js = compileXstate(contents, filename);
    process.stdout.write(js);
    process.stdout.write("\n");
  } catch {
    process.stderr.write("Compilation failed!\n");
    process.exit(1);
  }
}

run();