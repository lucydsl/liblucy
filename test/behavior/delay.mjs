// Delays: a timer started on entering a state and cleared on leaving it;
// delays in milliseconds, seconds, by an imported function and by a
// :symbol, which get the context and the event; a delay transition with
// actions; stop() clears the timers.
export const source = `
use './util.js' { calcDelay, note }

initial state green {
  delay(1s) => yellow
}

state yellow {
  go => green
  delay(500) => action(note) => red
}

state red {
  delay(calcDelay) => green
  delay(:named) => blue
}

state blue {}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const calcDelay = (context, event) => { log.push('calcDelay:' + event.type); return context.redMs; };
export const note = (context, event) => { log.push('note:' + event.type); };
`
};

export default function run({ mod, log, clock, assert }) {
  const seen = [];
  const machine = mod.default({
    context: { redMs: 2000 },
    delays: { named: (context, event) => { log.push('named:' + event.type); return 3000; } }
  });
  machine.subscribe(m => seen.push(m.state));
  machine.start();
  assert.equal(clock.pending, 1);

  clock.tick(999);
  assert.equal(machine.state, 'green');
  clock.tick(1);
  assert.equal(machine.state, 'yellow');

  // Leaving yellow by an event clears its timer.
  clock.tick(400);
  machine.send('go');
  assert.equal(machine.state, 'green');
  clock.tick(100);
  assert.equal(machine.state, 'green', 'the old yellow timer did not fire');
  assert.equal(clock.pending, 1);

  clock.tick(1000);
  assert.equal(machine.state, 'yellow');
  clock.tick(500);
  assert.equal(machine.state, 'red');
  assert.deepEqual(log.splice(0), ['note:delay', 'calcDelay:delay', 'named:delay'], 'both delays computed on entry, with the event');
  assert.equal(clock.pending, 2);

  // The shorter of red's two delays fires first and clears the other.
  clock.tick(2000);
  assert.equal(machine.state, 'green');
  assert.equal(clock.pending, 1);
  clock.tick(1500);
  assert.equal(machine.state, 'red');
  assert.equal(clock.pending, 2);
  assert.deepEqual(seen, ['green', 'yellow', 'green', 'yellow', 'red', 'green', 'yellow', 'red']);

  // stop() clears what is pending.
  machine.stop();
  assert.equal(clock.pending, 0);
  clock.tick(10000);
  assert.equal(machine.state, 'red');
  assert.equal(seen.length, 8, 'no notifications after stop');

  // The symbol delay path too.
  const other = mod.default({ context: { redMs: 9000 }, delays: { named: () => 10 } }).start();
  clock.tick(1500);
  assert.equal(other.state, 'red');
  clock.tick(10);
  assert.equal(other.state, 'blue');
  assert.equal(clock.pending, 0, "red's other timer was cleared");
}
