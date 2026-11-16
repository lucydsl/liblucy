// A machine's options are the ones its symbols are in, however it names
// them: through a named action or guard (`action log = :log`), in a
// machine nested in a state, or inline. Each used to be left out of the
// factory's parameters unless the same option was also named inline in a
// top-level state.
export const source = `
machine named {
  action log = :log
  guard ok = :ok
  action count = assign(n, :next)

  initial state idle {
    go => ok => log => count => busy
  }

  state busy {
    machine inner {
      initial state waiting {
        @entry => action(:enter)
        delay(:wait) => done
      }

      state done {}
    }
  }
}
`;

export default function run({ mod, clock, assert }) {
  const calls = [];
  const machine = mod.createNamed({
    context: { n: 1 },
    actions: { log: () => calls.push('log'), enter: () => calls.push('enter') },
    assigns: { next: context => context.n + 1 },
    delays: { wait: () => { calls.push('wait'); return 100; } },
    guards: { ok: () => { calls.push('ok'); return true; } }
  }).start();

  machine.send('go');
  assert.deepEqual(calls, ['ok', 'log', 'enter', 'wait']);
  assert.equal(machine.context.n, 2);
  assert.equal(machine.state, 'busy.waiting');
  clock.tick(100);
  assert.equal(machine.state, 'busy.done');
}
