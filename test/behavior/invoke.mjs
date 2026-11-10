// Invoking a promise: called with the context and the event on entering
// the state; done carries the value as event.data, error the reason; the
// state can be left and re-entered while a promise is pending, and the
// old promise's result is then ignored; a :symbol service comes from the
// options.
export const source = `
use './util.js' { getUser, setUser }

action assignUser = assign(user, setUser)

initial state loading {
  again => loading
  invoke(getUser) {
    done => assignUser => ready
    error => assign(reason) => failed
  }
}

state ready {
  more => more
}

state more {
  invoke(:fetchMore) {
    done => assign(more) => ready
  }
}

state failed {}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const pending = [];
export const getUser = (context, event) => {
  log.push('getUser:' + event.type);
  return new Promise((resolve, reject) => pending.push({ resolve, reject }));
};
export const setUser = (context, event) => event.data;
`
};

const settle = () => new Promise(resolve => setImmediate(resolve));

export default async function run({ mod, helpers, log, assert }) {
  const { pending } = helpers['./util.js'];
  const machine = mod.default({
    services: { fetchMore: (context, event) => Promise.resolve(context.user + '!') }
  });
  const seen = [];
  machine.subscribe(m => seen.push(m.state));
  machine.start();
  assert.deepEqual(log.splice(0), ['getUser:start']);
  assert.equal(pending.length, 1);

  // Leave and come back while the first promise is pending: a second call.
  machine.send('again');
  assert.deepEqual(log.splice(0), ['getUser:again']);
  assert.equal(pending.length, 2);

  // The first promise's result is ignored.
  pending[0].resolve('stale');
  await settle();
  assert.equal(machine.state, 'loading');
  assert.equal(machine.context.user, undefined);

  pending[1].resolve('anne');
  await settle();
  assert.equal(machine.state, 'ready');
  assert.equal(machine.context.user, 'anne');

  machine.send('more');
  assert.equal(machine.state, 'more');
  await settle();
  assert.equal(machine.state, 'ready');
  assert.equal(machine.context.more, 'anne!');
  assert.deepEqual(seen, ['loading', 'loading', 'ready', 'more', 'ready']);

  // Rejection.
  const other = mod.default({ services: { fetchMore: () => Promise.resolve() } }).start();
  pending[2].reject(new Error('nope'));
  await settle();
  assert.equal(other.state, 'failed');
  assert.equal(other.context.reason.message, 'nope');

  // A pending promise resolving after stop() does nothing.
  const stopped = mod.default({ services: { fetchMore: () => Promise.resolve() } }).start();
  stopped.stop();
  pending[3].resolve('late');
  await settle();
  assert.equal(stopped.state, 'loading');
  assert.equal(stopped.context.user, undefined);
}
