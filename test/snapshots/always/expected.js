import { createMachine } from 'xstate';

export default createMachine({
  initial: 'one',
  states: {
    one: {
      always: [
        {
          target: 'two'
        }
      ]
    },
    two: {
      type: 'final'
    }
  }
});
