import { createMachine } from 'xstate';
import { isDog } from './util';

export default function() {
  return createMachine({
    initial: 'idle',
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
