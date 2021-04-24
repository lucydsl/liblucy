#!/usr/bin/env node
import { promises as fsPromises } from 'fs';
const { readFile } = fsPromises;

const [,, filename] = process.argv;
const env = process.env.NODE_ENV || 'development';

if(!filename) {
  console.error('A filename is required')
  process.exit(1);
}

async function run() {
  const rel = env === 'production' ? 'prod' : 'dev';
  const [
    contents,
    { compileXstate, ready }
  ] = await Promise.all([
    readFile(filename, 'utf-8'),
    import(`../main-node-${rel}.mjs`)
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