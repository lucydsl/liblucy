#!/usr/bin/env node
import { promises as fsPromises } from 'fs';
const { readFile, writeFile } = fsPromises;

let index = 2;
let [,, filename] = process.argv;

let outFile = null;
let print = 'js';
const options = {};

while(filename && filename.startsWith('--')) {
  switch(filename) {
    case '--out-file': {
      outFile = process.argv[++index];
      break;
    }
    case '--ts': {
      options.format = 'ts';
      break;
    }
    case '--hydrate': {
      options.hydrate = true;
      break;
    }
    case '--target': {
      options.target = process.argv[++index];
      break;
    }
    case '--print': {
      print = process.argv[++index];
      break;
    }
  }
  filename = process.argv[++index];
}

if(!filename) {
  console.error('A filename is required')
  process.exit(1);
}

async function run() {
  const [
    contents,
    liblucy
  ] = await Promise.all([
    readFile(filename, 'utf-8'),
    import('../main-node.mjs')
  ]);
  await liblucy.ready;
  // compile is only set once ready resolves.
  const { compile } = liblucy;

  try {
    const result = compile(contents, filename, options);

    if(outFile) {
      await writeFile(outFile, result.ts ?? result.js, 'utf-8');
    } else {
      // The output ends in a newline already.
      if(result.ts !== undefined) {
        process.stdout.write(result.ts);
      } else if(print === 'dts') {
        process.stdout.write(result.dts);
      } else {
        process.stdout.write(result.js);
      }
    }
  } catch (err) {
    if(process.env.LUCY_DEBUG) console.error(err);
    process.stderr.write("Compilation failed!\n");
    process.exit(1);
  }
}

run();