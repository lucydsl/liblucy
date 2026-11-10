// The simplest machine: two states, one event, no options.
export const source = `
state enabled {
  toggle => disabled
}

initial state disabled {
  toggle => enabled
}
`;

export default function run({ mod, assert }) {
  const machine = mod.default();
  assert.equal(machine.state, 'disabled');
  assert.deepEqual(machine.context, {});

  const seen = [];
  const unsubscribe = machine.subscribe(m => seen.push(m.state));
  assert.equal(machine.start(), machine, 'start returns the machine');
  assert.deepEqual(seen, ['disabled'], 'start notifies');

  machine.send('toggle');
  assert.equal(machine.state, 'enabled');
  machine.send({ type: 'toggle' });
  assert.equal(machine.state, 'disabled');
  assert.deepEqual(seen, ['disabled', 'enabled', 'disabled']);

  machine.send('nothing');
  assert.equal(machine.state, 'disabled');
  assert.equal(seen.length, 3, 'an unhandled event does not notify');

  unsubscribe();
  machine.send('toggle');
  assert.equal(seen.length, 3, 'unsubscribed');

  // The context object the caller passed is the machine's context.
  const context = { count: 1 };
  assert.equal(mod.default({ context }).context, context);
}
