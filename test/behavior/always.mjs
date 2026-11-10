// Immediate transitions: taken on entering a state (from start too), the
// first whose guard passes; looked at again after a transition without a
// target changes the context; an unguarded one always goes.
export const source = `
use './util.js' { bigEnough, tick }

initial state counting {
  => guard(bigEnough) => big
  inc => assign(n, tick)
  reset => counting
}

state big {
  => action(:arrived) => rest
}

state rest {}
`;

export const modules = {
  './util.js': `
export const bigEnough = (context, event) => context.n >= 2;
export const tick = (context, event) => context.n + 1;
`
};

export default function run({ mod, log, assert }) {
  let machine = mod.default({ context: { n: 0 }, actions: { arrived: (context, event) => log.push('arrived:' + event.type) } });
  assert.equal(machine.state, 'counting');
  machine.start();
  assert.equal(machine.state, 'counting');

  machine.send('inc');
  assert.equal(machine.state, 'counting');
  machine.send('inc');
  assert.equal(machine.state, 'rest', 'the assign made the guard pass; big went straight on to rest');
  assert.equal(machine.context.n, 2);
  assert.deepEqual(log.splice(0), ['arrived:inc']);

  // Already big at start: goes through on start.
  machine = mod.default({ context: { n: 5 }, actions: { arrived: (context, event) => log.push('arrived:' + event.type) } }).start();
  assert.equal(machine.state, 'rest');
  assert.deepEqual(log.splice(0), ['arrived:start']);

  // Re-entering re-checks.
  machine = mod.default({ context: { n: 0 }, actions: { arrived: () => {} } }).start();
  machine.context = { n: 9 };
  machine.send('reset');
  assert.equal(machine.state, 'rest');
}
