import { createMachine } from 'xstate';

export const light = createMachine({
  initial: 'green',
  states: {
    green: {
      on: {
        timer: 'yellow'
      }
    },
    yellow: {
      on: {
        timer: 'red'
      }
    },
    red: {
      on: {
        timer: 'green'
      }
    }
  }
});

export const two = createMachine({
  states: {
    start: {
      on: {
        next: 'end'
      }
    },
    end: {
      type: 'final'
    }
  }
});
