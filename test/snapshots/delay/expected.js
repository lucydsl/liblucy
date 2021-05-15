import { createMachine } from 'xstate';
import { calcLightDelay } from './util';

export default function() {
  return createMachine({
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
}
