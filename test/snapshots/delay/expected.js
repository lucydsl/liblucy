import { Machine } from 'xstate';

export default Machine({
  initial: 'green',
  states: {
    green: {
      delay: {
        1000: 'yellow'
      }
    },
    yellow: {
      delay: {
        500: 'red'
      }
    },
    red: {
      delay: {
        2000: 'green'
      }
    }
  }
});
