// The variable for a machine's parent takes another name when the source
// imports something called parent, while the option keeps its name.
export const source = `
use './util.js' { parent }

machine kid {
  initial state idle {
    go => action(parent) => send(parent, went) => idle
  }
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const parent = () => { log.push('the import'); };
`
};

export default function run({ mod, log, assert }) {
  const got = [];
  const kid = mod.createKid({ parent: { send: event => got.push(event.type) } }).start();
  kid.send('go');
  assert.deepEqual(log.splice(0), ['the import']);
  assert.deepEqual(got, ['went']);
}
