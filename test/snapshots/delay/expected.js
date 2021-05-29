import { createMachine } from 'xstate';
import { calcLightDelay } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'green',
    context,
    states: {
      green: {
        after: {
          1000: 'yellow'
        }
      },
      yellow: {
        on: {
          go: 'green'
        },
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
