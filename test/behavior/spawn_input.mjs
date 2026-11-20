// Inputs: a block after a spawn gives what is spawned the context it
// starts with, each value a reference (the event's data, a key of the
// context, the machine itself, its parent) or a function called with the
// context and the event. An imported factory and a :symbol service get
// the inputs as their context option, beside the parent.
export const source = `
use './util.js' { makeId, makeProbe }

machine todoItem {
  input title(string)
  input storage
  input list
  input id

  initial state active {
    rename => assign(title) => active
    remove => removed
  }

  final state removed {
    @entry => send(list, removedTodo)
  }
}

machine plain {
  input owner
  input boss

  initial state only {}
}

machine todoList {
  action addDraft = assign(draft, spawn(todoItem) {
    title = :draftTitle
    storage = context.storage
    list = self
    id = makeId
  })

  initial state ready {
    add => assign(todo, spawn(todoItem) {
      title = event.data
      storage = context.storage
      list = self
      id = makeId
    }) => ready
    draft => addDraft => ready
    plain => assign(plain, spawn(plain) {
      owner = self
      boss = parent
    }) => ready
    probe => assign(probe, spawn(makeProbe) { label = event.data }) => assign(service, spawn(:probe) { label = context.storage }) => ready
    remove => send(todo, remove) => ready
    removedTodo => assign(removed, :yes) => ready
  }
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
let next = 1;
export const makeId = (context, event) => event.type + '-' + next++;
export const makeProbe = options => ({
  start() { log.push('probe ' + options.context.label + ' ' + (options.parent ? 'with' : 'without') + ' a parent'); return this; },
  stop() {},
  send() {}
});
`
};

export default function run({ mod, log, assert }) {
  const host = { send() {} };
  const list = mod.createTodoList({
    context: { storage: 'disk' },
    parent: host,
    assigns: { draftTitle: (context, event) => 'draft of ' + event.type, yes: () => true },
    services: {
      probe: ({ context, parent }) => ({
        start() { log.push('service ' + context.label + ' ' + (parent === list)); return this; },
        stop() {},
        send() {}
      })
    }
  }).start();

  // The event's data, a key of the context, the machine and a function.
  list.send({ type: 'add', data: 'milk' });
  const todo = list.context.todo;
  assert.deepEqual(todo.context, { title: 'milk', storage: 'disk', list, id: 'add-1' });
  assert.equal(todo.state, 'active', 'it was started');

  // The inputs are the context it starts with, which it goes on to change.
  todo.send({ type: 'rename', data: 'oat milk' });
  assert.equal(todo.context.title, 'oat milk');

  // In a named action, with a :symbol for a value.
  list.send('draft');
  assert.deepEqual(list.context.draft.context, { title: 'draft of draft', storage: 'disk', list, id: 'draft-2' });

  // self is how a child is told who to answer.
  list.send('remove');
  assert.equal(todo.state, 'removed');
  assert.equal(list.context.removed, true);

  // parent is the spawning machine's own parent. A machine that doesn't
  // use its parent isn't given one.
  list.send('plain');
  assert.deepEqual(list.context.plain.context, { owner: list, boss: host });
  assert.ok(!mod.createPlain.toString().includes('parent'));

  // An imported factory and a service get { context, parent }.
  list.send({ type: 'probe', data: 'imported' });
  assert.deepEqual(log.splice(0), ['probe imported with a parent', 'service disk true']);

  list.stop();
}
