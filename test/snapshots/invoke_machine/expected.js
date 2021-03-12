import { Machine } from 'xstate';

export const minute = Machine({
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

export const parent = Machine({
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
