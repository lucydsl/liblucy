// assign: with no value it stores event.data; with a function, the
// function's result; with a :symbol, the assigns option's; each makes a
// new context object, and later actions in the same transition see it.
export const source = `
use './util.js' { double, seeN }

action setDouble = assign(n, double)

initial state idle {
  set => assign(n) => action(seeN)
  twice => setDouble => action(seeN)
  named => assign(n, :fromOptions) => action(seeN)
  @exit => assign(left)
  go => done
}

state done {}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const double = (context, event) => context.n * 2;
export const seeN = (context, event) => { log.push(event.type + ':' + context.n); };
`
};

export default function run({ mod, log, assert }) {
  const first = { n: 1 };
  const machine = mod.default({
    context: first,
    assigns: { fromOptions: (context, event) => event.data + context.n }
  }).start();

  machine.send({ type: 'set', data: 5 });
  assert.equal(machine.context.n, 5);
  assert.notEqual(machine.context, first, 'assign makes a new object');
  assert.equal(first.n, 1, 'and leaves the old one alone');

  machine.send('twice');
  assert.equal(machine.context.n, 10);

  machine.send({ type: 'named', data: 7 });
  assert.equal(machine.context.n, 17);
  assert.deepEqual(log.splice(0), ['set:5', 'twice:10', 'named:17'], 'actions after an assign see the new context');

  machine.send({ type: 'go', data: 'bye' });
  assert.equal(machine.state, 'done');
  assert.equal(machine.context.left, 'bye', 'exit assign gets the event');
}
