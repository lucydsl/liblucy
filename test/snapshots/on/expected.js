import { createMachine } from 'xstate';

export default createMachine({
  initial: 'idle',
  states: {
    idle: {
      on: {
        purchase: 'end',
        delay: 'end',
        SNAKE_CASE: 'end',
        ANOTHER_SNAKE: 'end'
      }
    },
    end: {
      type: 'final'
    }
  }
});
