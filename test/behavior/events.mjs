// event declarations: they change nothing at run time. A machine that
// declares its events handles them as one that doesn't, and a name used
// as an event's type is not imported, so its module need not exist.
export const source = `
use './types.js' { Amount }
use './util.js' { positive, Label }

machine counter {
  event set(Amount)
  event rename(Label)
  event reset

  initial state idle {
    set => guard(positive) => assign(count)
    rename => assign(label)
    reset => assign(count, :zero) => idle
  }
}
`;

export const modules = {
  './util.js': `
export const positive = (context, event) => event.data > 0;
`
};

export default function run({ mod, assert }) {
  const machine = mod.createCounter({
    context: { count: 1, label: '' },
    assigns: { zero: () => 0 }
  }).start();

  machine.send({ type: 'set', data: 5 });
  assert.equal(machine.context.count, 5);
  machine.send({ type: 'set', data: -1 });
  assert.equal(machine.context.count, 5, 'the guard still sees the data');
  machine.send({ type: 'rename', data: 'five' });
  assert.equal(machine.context.label, 'five');
  machine.send('reset');
  assert.equal(machine.context.count, 0);
  assert.equal(machine.state, 'idle');
}
