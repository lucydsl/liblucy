import { createMachine } from 'xstate';

export default createMachine({
  initial: 'idle',
  states: {
    idle: {
      on: {
        purchase: 'end',
        delay: 'end'
      }
    },
    end: {
      type: 'final'
    }
  }
});
