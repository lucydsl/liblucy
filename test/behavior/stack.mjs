// The stack between the user's send and their action: send, then the
// state's handler, then the action. Nothing else.
export const source = `
use './util.js' { boom, increment }

action inc = assign(count, increment)

initial state active {
  go => action(boom)
  entry => entered
  inc => inc
}

state entered {
  @entry => action(boom)
}
`;

export const modules = {
  './util.js': `
export const boom = (context, event) => { throw new Error('boom'); };
export const increment = (context, event) => { throw new Error('boom'); };
`
};

// The frames of an error's stack between the function that threw and the
// caller of send, as "name (file:line)" strings.
function framesBetween(err, outer) {
  const lines = err.stack.split('\n').slice(1).map(line => line.trim());
  const end = lines.findIndex(line => line.includes(outer));
  return lines.slice(0, end === -1 ? lines.length : end).map(line => line.replace(/^at /, '').replace(/\(.*\/([^/]+)\)$/, '($1)'));
}

export default function run({ mod, assert }) {
  const machine = mod.default({ context: { count: 0 } }).start();

  let frames;
  try {
    machine.send('go');
  } catch(err) {
    frames = framesBetween(err, 'stack.mjs');
  }
  console.log('    event action:', frames.join(' <- '));
  assert.equal(frames.length, 3, 'boom <- handler <- send');
  assert.match(frames[1], /^go /);
  assert.match(frames[2], /send /);

  try {
    machine.send('inc');
  } catch(err) {
    frames = framesBetween(err, 'stack.mjs');
  }
  console.log('    assign:      ', frames.join(' <- '));
  assert.equal(frames.length, 3, 'increment <- handler <- send');

  try {
    machine.send('entry');
  } catch(err) {
    frames = framesBetween(err, 'stack.mjs');
  }
  console.log('    entry action:', frames.join(' <- '));
  assert.equal(frames.length, 4, 'boom <- enterEntered <- handler <- send');
  assert.match(frames[1], /enterEntered/);
}
