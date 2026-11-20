// Lists of machines: append(key, spawn(x)) adds what it spawns to the
// list the key holds, which need not exist yet; remove(key, value) stops
// a machine and takes it out, if it is in the list; stop() stops every
// machine in the list.
export const source = `
machine todoItem {
  input title
  input list

  initial state active {
    remove => removed
    delay(1s) => active
  }

  final state removed {
    @entry => send(list, removedTodo, self)
  }
}

machine todoList {
  action drop = remove(todos, context.selected)

  initial state ready {
    add => append(todos, spawn(todoItem) {
      title = event.data
      list = self
    }) => ready
    select => assign(selected) => ready
    removedTodo => remove(todos, event.data) => ready
    drop => drop => ready
  }
}
`;

export default function run({ mod, clock, assert }) {
  const list = mod.createTodoList().start();
  const seen = [];
  list.subscribe(m => seen.push((m.context.todos ?? []).map(todo => todo.context.title).join(',')));

  // The list is made by the first append, and each one adds to it.
  list.send({ type: 'add', data: 'milk' });
  list.send({ type: 'add', data: 'eggs' });
  list.send({ type: 'add', data: 'oats' });
  const [milk, eggs, oats] = list.context.todos;
  assert.deepEqual(seen.splice(0), ['milk', 'milk,eggs', 'milk,eggs,oats']);
  assert.equal(eggs.state, 'active', 'each was started');
  assert.equal(eggs.context.list, list);
  assert.equal(clock.pending, 3, 'each has its timer');

  // A todo that says it was removed is stopped and taken out. The list is
  // a new array each time, as the context is.
  const before = list.context.todos;
  eggs.send('remove');
  assert.deepEqual(list.context.todos, [milk, oats]);
  assert.notEqual(list.context.todos, before);
  assert.equal(clock.pending, 2, 'its timer was cleared');

  // By a key of the context, from a named action.
  list.send({ type: 'select', data: milk });
  list.send('drop');
  assert.deepEqual(list.context.todos, [oats]);
  assert.equal(clock.pending, 1);

  // A machine that isn't in the list is left alone: not stopped.
  const stranger = mod.createTodoItem({ context: { title: 'stranger', list: { send() {} } } }).start();
  list.send({ type: 'removedTodo', data: stranger });
  assert.deepEqual(list.context.todos, [oats]);
  assert.equal(clock.pending, 2, 'the stranger still has its timer');
  list.send({ type: 'removedTodo', data: undefined });
  stranger.stop();

  // remove before any append.
  const empty = mod.createTodoList().start();
  empty.send({ type: 'removedTodo', data: stranger });
  assert.equal(empty.context.todos, undefined);
  empty.stop();

  // A list the host starts it with is added to.
  const given = mod.createTodoList({ context: { todos: [stranger] } }).start();
  given.send({ type: 'add', data: 'rice' });
  assert.equal(given.context.todos.length, 2);
  assert.equal(given.context.todos[0], stranger);

  // stop() stops every machine in the list.
  list.send({ type: 'add', data: 'tea' });
  assert.equal(clock.pending, 3, 'oats, rice and tea');
  list.stop();
  given.stop();
  assert.equal(clock.pending, 0);
}
