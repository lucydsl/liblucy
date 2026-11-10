// spawn: a machine from this file, an imported factory or a :symbol
// service is created, started and stored in the context by an assign;
// send(actor, event) sends to it with the event's data; stop() stops them.
export const source = `
use './util.js' { makeCounter }

machine other {
  initial state only {
    run => assign(got) => only
  }
}

action makeThing = assign(second, spawn(other))
action sendThing = send(second, run)

initial state idle {
  event => assign(first, spawn(other)) => idle
  another => makeThing => idle
  ext => assign(counter, spawn(makeCounter)) => idle
  sym => assign(todo, spawn(:todoMachine)) => idle
  go => end
}

state end {
  click => send(first, run) => end
  dblclick => sendThing => end
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const makeCounter = () => ({
  started: false,
  start() { this.started = true; log.push('counter started'); return this; },
  stop() { log.push('counter stopped'); },
  send() {}
});
`
};

export default function run({ mod, log, assert }) {
  const machine = mod.default({
    services: { todoMachine: () => mod.createOther({ context: { kind: 'todo' } }) }
  }).start();

  machine.send('event');
  machine.send('another');
  const { first, second } = machine.context;
  assert.equal(first.state, 'only');
  assert.notEqual(first, second);

  machine.send('ext');
  assert.equal(machine.context.counter.started, true);
  machine.send('sym');
  assert.equal(machine.context.todo.context.kind, 'todo');
  assert.equal(machine.context.todo.state, 'only', 'the spawned service was started');
  assert.deepEqual(log.splice(0), ['counter started']);

  machine.send('go');
  machine.send({ type: 'click', data: 'to first' });
  assert.equal(first.context.got, 'to first');
  assert.equal(second.context.got, undefined);
  machine.send({ type: 'dblclick', data: 'to second' });
  assert.equal(second.context.got, 'to second');

  machine.stop();
  assert.deepEqual(log.splice(0), ['counter stopped']);
}
