Found a machine
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
