// The order actions run in: exit actions, then the transition's actions,
// then entry actions, each with the event; a transition to the state it
// is in leaves and re-enters it; one without a target doesn't; named
// actions, inline actions and :symbols are all called the same way.
export const source = `
use './util.js' { record }

action logIt = record

initial state a {
  @entry => logIt => action(record) => action(:sym)
  @exit => logIt
  go => logIt => b
  again => logIt => a
  stay => logIt
}

state b {
  @entry => logIt
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const record = (context, event) => { log.push('record:' + event.type + ':' + context.n); };
`
};

export default function run({ mod, log, assert }) {
  const machine = mod.default({
    context: { n: 1 },
    actions: {
      sym: (context, event) => log.push('sym:' + event.type)
    }
  });
  assert.deepEqual(log, [], 'nothing runs before start');
  machine.start();
  assert.deepEqual(log.splice(0), ['record:start:1', 'record:start:1', 'sym:start']);

  machine.send('stay');
  assert.equal(machine.state, 'a');
  assert.deepEqual(log.splice(0), ['record:stay:1'], 'no target: actions only');

  machine.send('again');
  assert.equal(machine.state, 'a');
  assert.deepEqual(log.splice(0), ['record:again:1', 'record:again:1', 'record:again:1', 'record:again:1', 'sym:again'],
    'a self transition exits, acts, and re-enters');

  machine.send('go');
  assert.equal(machine.state, 'b');
  assert.deepEqual(log.splice(0), ['record:go:1', 'record:go:1', 'record:go:1'], 'exit, action, entry');
}
