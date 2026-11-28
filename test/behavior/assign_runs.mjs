// A run of assigns, up to the first action that isn't one, is one copy of
// the context: each assign's function sees the ones before it, the
// context the machine had is never changed, an action after the run sees
// all of it, and if one throws, none of the run's assigns happened.
export const source = `
use './util.js' { first, second, boom, note }

machine runs {
  initial state idle {
    one => assign(d, first) => idle
    go => assign(a, first) => assign(b, second) => action(note) => assign(c, first) => idle
    fail => assign(a, first) => assign(b, boom) => idle
    twice => assign(a, first) => assign(a, second) => idle
  }
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const first = context => (context.a ?? 0) + 1;
// Reads a, which the assign before it in the run set.
export const second = context => context.a * 10;
export const boom = () => { throw new Error('boom'); };
export const note = context => { log.push(JSON.stringify(context)); };
`
};

export default function run({ mod, log, assert }) {
  const machine = mod.createRuns({ context: { a: 0 } }).start();
  const before = machine.context;
  machine.send('go');
  assert.equal(machine.context.a, 1);
  assert.equal(machine.context.b, 10, 'b read the a the run had just set');
  assert.equal(machine.context.c, 2, 'c is first again: a + 1');
  assert.deepEqual(log.splice(0), ['{"a":1,"b":10}'], 'the action saw the run before it');
  assert.deepEqual(before, { a: 0 }, 'the context the machine had is unchanged');

  // A throw in a run: none of it happened.
  const kept = machine.context;
  assert.throws(() => machine.send('fail'), /boom/);
  assert.equal(machine.context, kept, 'the context is the one before the run');

  // A key assigned twice in a run ends with the last.
  machine.send('twice');
  assert.equal(machine.context.a, 20, 'first made it 2, then second read that');

  // One copy per run; an assign on its own is inline.
  const text = mod.createRuns.toString();
  assert.equal(text.match(/\.\.\.machine\.context/g).length, 5, 'one copy for each run, and each assign alone');
  assert.equal(text.match(/const context = /g).length, 3, 'a block for each run of two or more');
  machine.send('one');
  assert.equal(machine.context.d, 21);
}
