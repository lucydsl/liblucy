import { Machine } from 'xstate';

export default Machine({
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
