// Nested machines: the state is the dotted path of the leaf; a nested
// machine starts in its initial state when its state is entered; events
// of the enclosing state work from any nested state; a nested state's own
// transition wins, unless its guards fail, when the enclosing state's is
// taken; a transition can go straight to a nested state; leaving the
// enclosing state runs the nested state's exit actions first.
export const source = `
use './util.js' { record, onlyFromWait }

initial state green {
  timer => yellow
  fix => red.wait
}

state yellow {
  timer => red
}

state red {
  timer => green
  @entry => action(record)
  @exit => action(record)

  machine pedestrian {
    initial state walk {
      countdown => wait
      @exit => action(record)
    }

    state wait {
      countdown => stop
      timer => guard(onlyFromWait) => walk
    }

    final state stop {}
  }
}
`;

export const modules = {
  './util.js': `
import { log } from './log.js';
export const record = (context, event) => { log.push(event.type); };
export const answers = { onlyFromWait: false };
export const onlyFromWait = () => answers.onlyFromWait;
`
};

export default function run({ mod, helpers, log, assert }) {
  const { answers } = helpers['./util.js'];
  const machine = mod.default().start();
  machine.send('timer');
  machine.send('timer');
  assert.equal(machine.state, 'red.walk');
  assert.deepEqual(log.splice(0), ['timer'], 'red was entered');

  machine.send('countdown');
  assert.equal(machine.state, 'red.wait');
  assert.deepEqual(log.splice(0), ['countdown'], 'walk was left');

  // wait's own timer transition, guarded out: red's is taken instead.
  machine.send('timer');
  assert.equal(machine.state, 'green');
  assert.deepEqual(log.splice(0), ['timer'], 'red was left');

  // Straight to a nested state.
  machine.send('fix');
  assert.equal(machine.state, 'red.wait');
  assert.deepEqual(log.splice(0), ['fix']);

  // Now wait's own transition wins.
  answers.onlyFromWait = true;
  machine.send('timer');
  assert.equal(machine.state, 'red.walk');
  assert.deepEqual(log.splice(0), []);

  machine.send('countdown');
  machine.send('countdown');
  assert.equal(machine.state, 'red.stop');
  assert.equal(machine.done, undefined, 'a nested final state does not finish the machine');
  machine.send('countdown');
  assert.equal(machine.state, 'red.stop');
  machine.send('timer');
  assert.equal(machine.state, 'green', 'the enclosing state still takes its events');
  assert.deepEqual(log.splice(0), ['countdown', 'timer']);
}
