// Invoking a machine from the same file: it is created and started on
// entering the state, its reaching a final state is the done event, and
// it is stopped when the state is left. A child that finishes as soon as
// it starts is noticed too.
export const source = `
machine minute {
  initial state active {
    delay(1s) => finished
  }

  final state finished {}
}

machine instant {
  initial state only {
    => fin
  }

  final state fin {}
}

machine parent {
  initial state pending {
    cancel => cancelled
    invoke(minute) {
      done => assign(when) => timesUp
    }
  }

  final state timesUp {}

  state cancelled {
    retry => pending
  }
}

machine quick {
  initial state waiting {
    invoke(instant) {
      done => over
    }
  }

  final state over {}
}
`;

export default function run({ mod, clock, assert }) {
  const parent = mod.createParent({ context: { when: 'never' } }).start();
  assert.equal(parent.state, 'pending');
  assert.equal(clock.pending, 1, 'the child machine is running');

  // Leaving pending stops the child.
  parent.send('cancel');
  assert.equal(parent.state, 'cancelled');
  assert.equal(clock.pending, 0, 'the child was stopped');
  clock.tick(5000);
  assert.equal(parent.state, 'cancelled');

  parent.send('retry');
  assert.equal(parent.state, 'pending');
  assert.equal(clock.pending, 1, 'a new child');
  clock.tick(1000);
  assert.equal(parent.state, 'timesUp');
  assert.equal(parent.done, true);
  assert.equal(parent.context.when, undefined, 'done from a machine carries no data');

  // stop() stops a running child.
  const another = mod.createParent().start();
  assert.equal(clock.pending, 1);
  another.stop();
  assert.equal(clock.pending, 0);

  // A child that is done as soon as it starts.
  const quick = mod.createQuick().start();
  assert.equal(quick.state, 'over');
  assert.equal(quick.done, true);

  // The children are ordinary machines of this module.
  const minute = mod.createMinute().start();
  clock.tick(1000);
  assert.equal(minute.state, 'finished');
  assert.equal(minute.done, true);
}
