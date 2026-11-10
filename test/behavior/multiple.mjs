// Several machines in one file are independent exports; a machine with
// no initial state starts in its first; a machine with no states at all
// still has the API.
export const source = `
machine light {
  initial state green {
    timer => yellow
  }

  state yellow {
    timer => green
  }
}

machine two {
  state start {
    next => end
  }

  final state end {}
}

machine empty {}
`;

export default function run({ mod, assert }) {
  assert.deepEqual(Object.keys(mod).sort(), ['createEmpty', 'createLight', 'createTwo']);
  const light = mod.createLight().start();
  const two = mod.createTwo().start();
  assert.equal(two.state, 'start');
  light.send('timer');
  assert.equal(light.state, 'yellow');
  assert.equal(two.state, 'start');
  two.send('next');
  assert.equal(two.state, 'end');
  assert.equal(two.done, true);
  assert.equal(light.state, 'yellow');

  const empty = mod.createEmpty().start();
  assert.equal(empty.state, undefined);
  empty.send('anything');
  empty.stop();
}
