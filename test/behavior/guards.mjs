// Several transitions on one event, taken in order by their guards; an
// immediate transition with a guard; several guards on one transition,
// all of which must pass; final states.
export const source = `
use './util.js' { isValidCard, isInvalidCard, moneyWithdrawn, check }

guard isMoneyWithdrawn = moneyWithdrawn
guard canGo = check
guard sureCanGo = check

initial state idle {
  next => guard(isValidCard) => purchase
  next => guard(isInvalidCard) => error
  go => {
    canGo =>
      sureCanGo =>
      purchased
  }
}

state purchase {
  => isMoneyWithdrawn => purchased
}

final state purchased {}

final state error {}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const answers = { valid: false, invalid: false, withdrawn: false, check: [] };
export const isValidCard = (context, event) => { log.push('isValidCard:' + event.type); return answers.valid; };
export const isInvalidCard = (context, event) => { log.push('isInvalidCard:' + event.type); return answers.invalid; };
export const moneyWithdrawn = (context, event) => { log.push('moneyWithdrawn:' + event.type); return answers.withdrawn; };
export const check = (context, event) => { log.push('check:' + event.type); return answers.check.shift(); };
`
};

export default function run({ mod, helpers, log, assert }) {
  const { answers } = helpers['./util.js'];

  // No guard passes: nothing happens, but both were asked.
  let machine = mod.default().start();
  machine.send('next');
  assert.equal(machine.state, 'idle');
  assert.deepEqual(log.splice(0), ['isValidCard:next', 'isInvalidCard:next']);

  // The second passes.
  answers.invalid = true;
  machine.send('next');
  assert.equal(machine.state, 'error');
  assert.equal(machine.done, true);
  assert.deepEqual(log.splice(0), ['isValidCard:next', 'isInvalidCard:next']);

  // The first passes, so the second isn't asked; purchase's immediate
  // transition is guarded and fails, so it stays.
  machine = mod.default().start();
  answers.valid = true;
  machine.send('next');
  assert.equal(machine.state, 'purchase');
  assert.equal(machine.done, false);
  assert.deepEqual(log.splice(0), ['isValidCard:next', 'moneyWithdrawn:next']);

  // ...and passes when the guard does.
  answers.withdrawn = true;
  machine = mod.default().start();
  machine.send('next');
  assert.equal(machine.state, 'purchased');
  assert.equal(machine.done, true);

  // Final states take no events.
  machine.send('next');
  assert.equal(machine.state, 'purchased');
  assert.deepEqual(log.splice(0), ['isValidCard:next', 'moneyWithdrawn:next']);

  // Several guards: all have to pass, and they stop at the first that fails.
  machine = mod.default().start();
  answers.check = [true, false];
  machine.send('go');
  assert.equal(machine.state, 'idle');
  assert.deepEqual(log.splice(0), ['check:go', 'check:go']);
  answers.check = [false, true];
  machine.send('go');
  assert.equal(machine.state, 'idle');
  assert.deepEqual(log.splice(0), ['check:go']);
  answers.check = [true, true];
  machine.send('go');
  assert.equal(machine.state, 'purchased');
}
