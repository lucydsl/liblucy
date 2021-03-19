import { Machine } from 'xstate';
import { calcLightDelay } from './util';

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
        calcLightDelay: 'green'
      }
    }
  }
}, {
  delays: {
    calcLightDelay: calcLightDelay
  }
});
