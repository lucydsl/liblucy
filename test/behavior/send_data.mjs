// send(actor, event, value): the event carries the value, a reference as
// in a spawn's block, in place of the data of the event being handled.
// self is how a parent learns which machine sent an event.
export const source = `
use './util.js' { describe }

machine todoItem {
  input id
  input list

  action tellRemoved = send(list, removed, self)

  initial state active {
    rename => send(list, renamed, event.data) => active
    touch => send(parent, touched, context.id) => active
    report => send(parent, report, describe) => active
    forward => send(parent, forwarded) => active
    up => send(list, boss, parent) => active
    remove => tellRemoved => gone
  }

  final state gone {}
}

machine reporter {
  initial state only {
    report => send(parent, summary, :summarise) => only
  }
}

machine todoList {
  initial state ready {
    add => assign(todo, spawn(todoItem) {
      id = event.data
      list = self
    }) => ready
    tell => send(todo, rename, context.name) => ready
    touched => assign(touched) => ready
    renamed => assign(renamed) => ready
    removed => assign(removed) => ready
    boss => assign(boss) => ready
  }
}
`;

export const modules = {
  './util.js': `
export const describe = (context, event) => event.type + ' of ' + context.id;
`
};

export default function run({ mod, assert }) {
  const list = mod.createTodoList({ context: { name: 'oats' } }).start();
  list.send({ type: 'add', data: 7 });
  const todo = list.context.todo;

  // A key of the sender's context, both ways.
  todo.send('touch');
  assert.equal(list.context.touched, 7);
  list.send('tell');
  assert.equal(list.context.renamed, 'oats', 'the todo got the name and sent it on as event.data');

  // The machine itself, from a named action.
  todo.send('remove');
  assert.equal(list.context.removed, todo);

  // On its own: every kind of value, to the parent given as an option.
  const got = [];
  const host = { send: event => got.push(event) };
  const alone = mod.createTodoItem({
    context: { id: 3, list: host },
    parent: host
  }).start();
  alone.send({ type: 'rename', data: 'milk' });
  alone.send('touch');
  alone.send({ type: 'report', data: 'ignored' });
  alone.send({ type: 'forward', data: 'passed on' });
  alone.send('up');
  alone.send({ type: 'remove', data: 'ignored' });
  assert.deepEqual(got, [
    { type: 'renamed', data: 'milk' },
    { type: 'touched', data: 3 },
    { type: 'report', data: 'report of 3' },
    { type: 'forwarded', data: 'passed on' },
    { type: 'boss', data: host },
    { type: 'removed', data: alone }
  ]);

  // A :symbol, from the assigns option.
  const reporter = mod.createReporter({
    parent: host,
    assigns: { summarise: (context, event) => 'summary of ' + event.data }
  }).start();
  got.length = 0;
  reporter.send({ type: 'report', data: 'today' });
  assert.deepEqual(got, [{ type: 'summary', data: 'summary of today' }]);
}
