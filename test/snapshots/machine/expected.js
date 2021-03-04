import { Machine } from 'xstate';

export const light = Machine({
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
export const two = Machine({
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
