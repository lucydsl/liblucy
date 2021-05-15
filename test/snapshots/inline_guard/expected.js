import { createMachine } from 'xstate';
import { isDog } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
    states: {
      idle: {
        on: {
          pet: {
            target: 'pet',
            cond: isDog
          }
        }
      },
      pet: {
        always: [
          {
            target: 'goodBoy'
          }
        ]
      },
      goodBoy: {
        type: 'final'
      }
    }
  });
}
