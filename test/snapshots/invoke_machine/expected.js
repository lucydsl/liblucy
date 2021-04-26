import { createMachine } from 'xstate';

export const minute = createMachine({
  initial: 'active',
  states: {
    active: {
      on: {
        timer: 'finished'
      }
    },
    finished: {
      type: 'final'
    }
  }
});

export const parent = createMachine({
  initial: 'pending',
  states: {
    pending: {
      invoke: {
        src: minute,
        onDone: 'timesUp'
      }
    },
    timesUp: {
      type: 'final'
    }
  }
});
