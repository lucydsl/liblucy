import { Machine } from 'xstate';

export default Machine({
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

    }
  }
});
