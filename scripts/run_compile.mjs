import { compileXstate as compile } from '../liblucy.mjs';

process.stdin.setEncoding('utf-8');

let lucyStr = '';
process.stdin.on('data', str => {
  lucyStr += str;
})

process.stdin.on('end', () => {
  const js = compile(lucyStr);
  console.log(js);
});