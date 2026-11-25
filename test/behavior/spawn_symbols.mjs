// Symbols at a spawn: a `:name = function` line in a spawn's block gives
// the machine it spawns one of its :symbols, a function from use or one
// of the spawning machine's own. The function is handed over, not called,
// and is in the option the spawned machine uses it in, never its context.
// A machine can be given a symbol only to pass it on.
export const source = `
use './util.js' { log, save }

machine subtask {
  initial state open {
    close => guard(:canClose) => closed
  }

  final state closed {}
}

machine todoItem {
  input title

  initial state active {
    remove => guard(:canDelete) => action(:log) => assign(reason, :why) => removed
    split => assign(sub, spawn(subtask) { :canClose = :canCloseSub }) => active
    keep => saving
  }

  state saving {
    invoke(:save) {
      done => assign(saved) => active
    }
  }

  state removed {
    delay(:wait) => active
  }
}

machine todoList {
  initial state ready {
    add => guard(:canAdd) => append(todos, spawn(todoItem) {
      title = event.data
      :canDelete = :canDeleteTodo
      :log = log
      :why = :reasonFor
      :wait = :todoWait
      :save = save
      :canCloseSub = :canAdd
    }) => ready
  }
}
`;

export const modules = {
  './util.js': `
import { log as lines } from './log.js';
export const log = (context, event) => { lines.push('log ' + event.type + ' ' + context.title); };
export const save = (context, event) => Promise.resolve('saved ' + context.title);
`
};

const settle = () => new Promise(resolve => setImmediate(resolve));

export default async function run({ mod, log, clock, assert }) {
  const calls = [];
  let allowDelete = false;
  const list = mod.createTodoList({
    delays: { todoWait: () => 250 },
    guards: {
      canAdd: (context, event) => { calls.push('canAdd ' + event.type); return true; },
      canDeleteTodo: (context, event) => { calls.push('canDelete ' + context.title); return allowDelete; }
    },
    assigns: { reasonFor: (context, event) => 'because ' + event.data }
  }).start();

  list.send({ type: 'add', data: 'milk' });
  const todo = list.context.todos[0];
  assert.deepEqual(calls.splice(0), ['canAdd add'], 'nothing given to the todo was called at the spawn');
  assert.deepEqual(todo.context, { title: 'milk' }, 'the functions are not in its context');

  // A guard, called with the todo's context, not the list's.
  todo.send({ type: 'remove', data: 'it went off' });
  assert.equal(todo.state, 'active');
  assert.deepEqual(calls.splice(0), ['canDelete milk']);

  // A symbol the todo has only to pass on: the list's canAdd is the
  // subtask's canClose.
  todo.send('split');
  todo.context.sub.send('close');
  assert.deepEqual(calls.splice(0), ['canAdd close']);
  assert.equal(todo.context.sub.state, 'closed');

  // A service, from an imported function.
  todo.send('keep');
  await settle();
  assert.equal(todo.context.saved, 'saved milk');

  // An action from use, an assign's value and a delay.
  allowDelete = true;
  todo.send({ type: 'remove', data: 'it went off' });
  assert.equal(todo.state, 'removed');
  assert.deepEqual(log.splice(0), ['log remove milk']);
  assert.equal(todo.context.reason, 'because it went off');
  clock.tick(250);
  assert.equal(todo.state, 'active');

  list.stop();
}
