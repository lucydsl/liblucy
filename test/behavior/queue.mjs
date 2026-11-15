// The queue: an event sent to a machine while it is handling one (from an
// action, a listener, or an actor it sent to) waits until that transition
// and its notification are over, then is handled, in order, before the
// outermost send returns. The same goes for events sent during a timer's
// transition and during start(). An error clears the queue and leaves the
// machine usable. A machine that calls nothing of its own has no queue.
export const source = `
use './util.js' { makeEcho }

machine form {
  initial state editing {
    submit => action(:save) => saving
  }

  state saving {
    saved => stored
    failed => editing
    delay(5s) => action(:timedOut) => editing
  }

  state stored {
    reset => action(:boom) => editing
    again => editing
    stop => action(:halt) => editing
  }
}

machine eager {
  initial state first {
    @entry => action(:kick)
    next => second
  }

  state second {
    next => third
  }

  state third {}
}

machine caller {
  initial state idle {
    @entry => assign(echo, spawn(makeEcho))
    ask => send(echo, ping) => action(:note) => waiting
  }

  state waiting {
    pong => idle
  }
}

machine plain {
  initial state off {
    toggle => on
  }

  state on {
    toggle => off
  }
}
`;

export const modules = {
  './util.js': `
export const link = {};
// An actor that answers a ping at once, to whatever link.machine is.
export const makeEcho = () => ({
  start() { return this; },
  stop() {},
  send(event) {
    if(event.type === 'ping') {
      link.machine.send('pong');
    }
  }
});
`
};

export default function run({ mod, helpers, log, clock, assert }) {
  // An action that sends: the event is handled once the machine is in the
  // new state, where there is a transition for it.
  let form;
  const seen = [];
  const actions = {
    save: () => { form.send('saved'); log.push('save returned in ' + form.state); },
    timedOut: () => { form.send('submit'); log.push('timedOut returned in ' + form.state); },
    boom: () => { form.send('submit'); throw new Error('boom'); },
    halt: () => { form.send('submit'); form.stop(); }
  };
  form = mod.createForm({ actions }).start();
  form.subscribe(m => seen.push(m.state));
  form.send('submit');
  assert.equal(form.state, 'stored', 'saved was handled in saving, before send returned');
  assert.deepEqual(log.splice(0), ['save returned in editing']);
  assert.deepEqual(seen.splice(0), ['saving', 'stored'], 'a notification for each event');
  assert.equal(clock.pending, 0, 'leaving saving cleared its timer');

  // An error clears the queue, and the machine goes on working.
  assert.throws(() => form.send('reset'), /boom/);
  assert.equal(form.state, 'stored');
  form.send('again');
  assert.equal(form.state, 'editing', 'the submit queued before the error was dropped');

  // A timer's transition queues too.
  actions.save = () => {};
  form.send('submit');
  assert.equal(form.state, 'saving');
  clock.tick(5000);
  assert.deepEqual(log.splice(0), ['timedOut returned in saving']);
  assert.equal(form.state, 'saving', 'the submit sent by the timer\'s action was handled in editing');
  assert.deepEqual(seen.splice(0), ['editing', 'saving', 'editing', 'saving']);

  // A listener that sends: every listener hears of one event before any
  // hears of the next.
  const order = [];
  const one = mod.createForm({ actions: { save: () => {} } }).start();
  one.subscribe(m => {
    order.push('a:' + m.state);
    if(m.state === 'saving') {
      m.send('failed');
    }
  });
  one.subscribe(m => order.push('b:' + m.state));
  one.send('submit');
  assert.deepEqual(order, ['a:saving', 'b:saving', 'a:editing', 'b:editing']);

  // stop() during a transition drops what is queued.
  form.send('saved');
  form.send('stop');
  assert.equal(form.state, 'editing', 'the queued submit was dropped by stop()');

  // Events sent during start() are handled after the initial state is entered.
  let eager;
  eager = mod.createEager({ actions: { kick: () => { eager.send('next'); eager.send('next'); } } });
  const states = [];
  eager.subscribe(m => states.push(m.state));
  eager.start();
  assert.deepEqual(states, ['first', 'second', 'third']);

  // An actor that answers during the send to it: the answer waits for the
  // transition that asked.
  const caller = mod.createCaller({ actions: { note: () => log.push('asked in ' + caller.state) } });
  helpers['./util.js'].link.machine = caller;
  caller.start();
  caller.send('ask');
  assert.deepEqual(log.splice(0), ['asked in idle']);
  assert.equal(caller.state, 'idle', 'pong was handled in waiting');

  // A machine that only changes state is as it was: no queue.
  const text = mod.createPlain.toString();
  assert.ok(!text.includes('queue'), 'no queue in a machine that calls nothing');
  assert.ok(mod.createForm.toString().includes('queue'));
}
