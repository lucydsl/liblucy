import { Machine } from 'xstate';
import { calcLightDelay } from './util';

export default Machine({
  initial: 'green',
  states: {
    green: {
      after: {
        1000: 'yellow'
      }
    },
    yellow: {
      after: {
        500: 'red'
      }
    },
    red: {
      after: {
        calcLightDelay: 'green'
      }
    }
  }
}, {
  delays: {
    calcLightDelay: calcLightDelay
  }
});
