// The queue's own variables (busy, queue, run) take other names when the
// source imports something called that, so the imports still work: no
// warning (the harness fails on one), and the queue works as well.
export const source = `
use './util.js' { run, queue, busy }

machine job {
  initial state idle {
    @entry => action(busy)
    go => action(run) => working
  }

  state working {
    finish => idle
    delay(1s) => action(queue) => idle
  }
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const link = {};
export const busy = () => { log.push('busy'); };
export const run = () => { log.push('run'); link.machine.send('finish'); };
export const queue = () => { log.push('queue'); };
`
};

export default function run({ mod, helpers, log, clock, assert }) {
  const job = mod.createJob();
  helpers['./util.js'].link.machine = job;
  job.start();
  assert.deepEqual(log.splice(0), ['busy']);

  // The imported run is called, and the finish it sends is queued.
  job.send('go');
  assert.deepEqual(log.splice(0), ['run', 'busy']);
  assert.equal(job.state, 'idle');

  helpers['./util.js'].link.machine = { send() {} };
  job.send('go');
  assert.equal(job.state, 'working');
  clock.tick(1000);
  assert.deepEqual(log.splice(0), ['run', 'queue', 'busy']);
  assert.equal(job.state, 'idle');

  const text = mod.createJob.toString();
  assert.ok(text.includes('function run2(step'));
  assert.ok(text.includes('queue2.push(event)') && text.includes('busy2 = true'));
}
