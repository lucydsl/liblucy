#!/usr/bin/env node
import { promises as fsPromises } from 'fs';
const { readFile, writeFile } = fsPromises;

let index = 2;
let [,, filename] = process.argv;
const env = process.env.NODE_ENV || 'development';

let outFile = null;
let print = 'js';
const options = {
  useRemote: false,
  dts: false
};

while(filename && filename.startsWith('--')) {
  switch(filename) {
    case '--remote-imports': {
      options.useRemote = true;
      break;
    }
    case '--out-file': {
      outFile = process.argv[++index];
      break;
    }
    case '--experimental-dts': {
      options.dts = true;
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
  const rel = env === 'production' ? 'prod' : 'dev';
  const [
    contents,
    liblucy
  ] = await Promise.all([
    readFile(filename, 'utf-8'),
    import(`../main-node-${rel}.mjs`)
  ]);
  await liblucy.ready;
  // compileXstate is only set once ready resolves.
  const { compileXstate } = liblucy;

  try {
    const result = compileXstate(contents, filename, options);

    if(outFile) {
      await writeFile(outFile, result.js, 'utf-8');
    } else {
      if(options.dts && print === 'dts') {
        process.stdout.write(result.dts);
      } else {
        process.stdout.write(result.js);
      }

      process.stdout.write("\n");
    }
  } catch (err) {
    if(process.env.LUCY_DEBUG) console.error(err);
    process.stderr.write("Compilation failed!\n");
    process.exit(1);
  }
}

run();