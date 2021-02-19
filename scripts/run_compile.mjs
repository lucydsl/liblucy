import { compileXstate as compile } from '../liblucy.mjs';
import { readFileSync } from 'fs';

const filename = process.argv[2];

if(!filename) {
  console.log('Filename is required.');
  process.exit(1);
}

const source = readFileSync(filename, 'utf-8');

try {
  const js = compile(source, filename);
  console.log(js);
} catch {
  console.error('Compilation failed!');
}