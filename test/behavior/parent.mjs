// A machine's parent is whatever created it: send(parent, event) sends
// to the machine that spawned or invoked it, which handles the event
// after the transition it is in. A machine nothing created takes its
// parent as an option, and without one the send does nothing. What a
// spawn creates is given its parent, and spawning into a key again stops
// what the key held.
export const source = `
use './util.js' { makeProbe }

machine todoItem {
  initial state active {
    remove => removed
    rename => send(parent, renamed) => active
    delay(1s) => active
  }

  final state removed {
    @entry => send(parent, removedTodo)
  }
}

machine todoList {
  initial state ready {
    add => assign(todo, spawn(todoItem)) => ready
    rename => send(todo, rename) => ready
    renamed => assign(name) => ready
    remove => send(todo, remove) => action(:note) => ready
    removedTodo => empty
    probe => assign(probe, spawn(makeProbe)) => assign(service, spawn(:probe)) => ready
  }

  state empty {}
}

machine countdown {
  initial state ticking {
    delay(1s) => send(parent, tick) => over
  }

  final state over {}
}

machine watcher {
  initial state watching {
    tick => assign(ticked, :yes)
    invoke(countdown) {
      done => finished
    }
  }

  state finished {}
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const makeProbe = options => ({
  start() { log.push('probe parent: ' + (options.parent ? options.parent.state : 'none')); return this; },
  stop() {},
  send() {}
});
`
};

export default function run({ mod, log, clock, assert }) {
  let list;
  list = mod.createTodoList({
    actions: { note: () => log.push('remove sent in ' + list.state) },
    services: { probe: ({ parent }) => ({ start() { log.push('service parent is the list: ' + (parent === list)); return this; }, stop() {}, send() {} }) }
  }).start();

  // The child's event reaches its parent, with its data.
  list.send('add');
  const first = list.context.todo;
  list.send({ type: 'rename', data: 'milk' });
  assert.equal(list.context.name, 'milk');

  // Spawning into the key again stops the machine it held.
  assert.equal(clock.pending, 1, 'the first todo has a timer');
  list.send('add');
  assert.notEqual(list.context.todo, first);
  assert.equal(clock.pending, 1, 'the first todo was stopped, and the second started');

  // The child answers during the send to it, and the parent handles the
  // answer once its own transition is over.
  list.send('remove');
  assert.deepEqual(log.splice(0), ['remove sent in ready']);
  assert.equal(list.context.todo.state, 'removed');
  assert.equal(list.state, 'empty');

  // An imported factory and a :symbol service are given the parent.
  const other = mod.createTodoList({
    actions: { note() {} },
    services: { probe: ({ parent }) => ({ start() { log.push('service parent is the list: ' + (parent === other)); return this; }, stop() {}, send() {} }) }
  }).start();
  other.send('probe');
  assert.deepEqual(log.splice(0), ['probe parent: ready', 'service parent is the list: true']);
  other.stop();

  // A machine nothing created: no parent, and the send does nothing.
  const alone = mod.createTodoItem().start();
  alone.send('rename');
  alone.send('remove');
  assert.equal(alone.state, 'removed');

  // Or its creator passes itself as the parent.
  const got = [];
  const owned = mod.createTodoItem({ parent: { send: event => got.push(event) } }).start();
  owned.send({ type: 'rename', data: 'eggs' });
  owned.send('remove');
  assert.deepEqual(got, [{ type: 'renamed', data: 'eggs' }, { type: 'removedTodo', data: undefined }]);
  owned.stop();
  alone.stop();
  list.stop();
  assert.equal(clock.pending, 0);

  // An invoked machine has its parent too.
  const watcher = mod.createWatcher({ assigns: { yes: () => true } }).start();
  clock.tick(1000);
  assert.equal(watcher.context.ticked, true, 'tick was handled before done');
  assert.equal(watcher.state, 'finished');
}
