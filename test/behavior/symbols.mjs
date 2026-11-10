// Everything supplied through the options: actions, assigns, guards,
// delays and services, called with the context and the event.
export const source = `
action logger = :doLog
guard isValid = :checkValid

initial state idle {
  next => isValid => guard(:check) => action(:log) => assign(name, :namer) => loading
}

state loading {
  @entry => action(:updateUI) => action(:log) => assign(count, :incrementLoads)

  invoke(:loadUsers) {
    done => loaded
  }
}

state loaded {
  delay(:wait) => logger => homescreen

  @exit => action(:log)
}

state homescreen {
  @entry => assign(todo, spawn(:todoMachine))
}
`;

const settle = () => new Promise(resolve => setImmediate(resolve));

export default async function run({ mod, log, clock, assert }) {
  const calls = [];
  const note = name => (context, event) => calls.push(`${name}(${event.type}, count=${context.count})`);
  const machine = mod.default({
    context: { count: 0 },
    actions: {
      doLog: note('doLog'),
      log: note('log'),
      updateUI: note('updateUI')
    },
    assigns: {
      namer: (context, event) => event.data,
      incrementLoads: (context, event) => context.count + 1
    },
    guards: {
      checkValid: (context, event) => { calls.push('checkValid'); return true; },
      check: (context, event) => { calls.push('check'); return true; }
    },
    delays: {
      wait: (context, event) => { calls.push('wait'); return 250; }
    },
    services: {
      loadUsers: (context, event) => { calls.push(`loadUsers(${event.type})`); return Promise.resolve(['a']); },
      todoMachine: () => ({ start() { calls.push('todo started'); return this; }, stop() {}, send() {} })
    }
  }).start();

  machine.send({ type: 'next', data: 'anne' });
  assert.equal(machine.state, 'loading');
  assert.equal(machine.context.name, 'anne');
  assert.equal(machine.context.count, 1);
  await settle();
  assert.equal(machine.state, 'loaded');
  clock.tick(250);
  assert.equal(machine.state, 'homescreen');
  assert.deepEqual(calls, [
    'checkValid', 'check', 'log(next, count=0)',
    'updateUI(next, count=0)', 'log(next, count=0)', 'loadUsers(next)',
    'wait',
    'log(delay, count=1)', 'doLog(delay, count=1)',
    'todo started'
  ]);
  void log;
}
