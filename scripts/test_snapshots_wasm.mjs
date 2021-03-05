import { ready, compileXstate } from '../main-node-dev.mjs';
import { join as pathJoin, dirname as pathDirname } from 'path';
import { promises as fsPromises } from 'fs';
const { readFile, readdir } = fsPromises;

const dirname = pathDirname(import.meta.url.substr(7));
const projectRoot = pathJoin(dirname, '..');
const snapshotRoot = pathJoin(projectRoot, 'test/snapshots');

let update = false;
let runAll = true;
let testFolder;

async function testSnapshot(folder) {
  const filename = pathJoin(folder, 'input.lucy');
  const input = await readFile(filename, 'utf-8');
  const expected = pathJoin(folder, 'expected.js');

  const output = compileXstate(input, filename);
  console.log(output);
}

switch(process.argv.length) {
  case 3: {
    switch(process.argv[2]) {
      case '-u': {
        update = true;
        break;
      }
      default: {
        runAll = false;
        testFolder = process.argv[2];
        break;
      }
    }
    break;
  }
  case 4: {
    switch(process.argv[2]) {
      case '-u': {
        update = true;
        break;
      }
      default: break;
    }
    runAll = false;
    testFolder = process.argv[3];
    break;
  }
  default: break;
}

async function testAllSnapshots() {
  const folders = await readdir(snapshotRoot);
  
  for(const folder of folders) {
    const snapshotFolder = pathJoin(snapshotRoot, folder);
    await testSnapshot(snapshotFolder);
  }
}

async function run() {
  await ready;

  if(runAll) {
    await testAllSnapshots();
  } else {
    await testSnapshot(pathJoin(projectRoot, testFolder));
  }
}

run();