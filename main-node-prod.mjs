import createModule from './dist/liblucy-release-node.mjs';
import init from './liblucy.mjs';

export let compile;

export let ready = init(createModule).then(exports => {
  compile = exports.compile;
});