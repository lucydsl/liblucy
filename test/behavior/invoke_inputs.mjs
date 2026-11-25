// An invoked machine is given what a spawned one is, in the invoke's
// block beside its transitions: inputs, which are the context it starts
// with, and symbols. It still lives as long as the state, and what it
// sends its parent the state handles as its own events.
export const source = `
use './util.js' { describe }

machine countdown {
  input seconds
  input label

  initial state ticking {
    delay(1s) => guard(:more) => send(parent, tick, context.label) => ticking
    delay(3s) => finished
  }

  final state finished {}
}

machine timer {
  initial state idle {
    start => running
  }

  state running {
    invoke(countdown) {
      seconds = event.data
      label = describe
      :more = :keepGoing
      done => assign(finished, :yes) => idle
    }
    tick => assign(last)
    cancel => idle
  }
}
`;

export const modules = {
  './util.js': `
export const describe = (context, event) => event.type + ' for ' + event.data;
`
};

export default function run({ mod, clock, assert }) {
  const calls = [];
  let more = true;
  const timer = mod.createTimer({
    assigns: { yes: () => true },
    guards: { keepGoing: (context, event) => { calls.push('more? ' + context.seconds); return more; } }
  }).start();
  assert.equal(clock.pending, 0, 'nothing is invoked before the state is entered');

  // The inputs are worked out as the state is entered, from the event
  // that entered it, and the symbol is the child's guard.
  timer.send({ type: 'start', data: 3 });
  assert.equal(timer.state, 'running');
  clock.tick(1000);
  assert.deepEqual(calls.splice(0), ['more? 3'], 'called with the context of the child');
  assert.equal(timer.context.last, 'start for 3', 'the child sent its label, which a function made');

  // Leaving the state stops the machine.
  timer.send('cancel');
  assert.equal(clock.pending, 0);

  // Entering it again invokes a new one, with new inputs, and its
  // reaching a final state is done.
  more = false;
  timer.send({ type: 'start', data: 9 });
  clock.tick(1000);
  assert.deepEqual(calls.splice(0), ['more? 9']);
  clock.tick(2000);
  assert.equal(timer.state, 'idle');
  assert.equal(timer.context.finished, true);
  assert.equal(clock.pending, 0);
}
