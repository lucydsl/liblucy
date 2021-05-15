import { createMachine } from 'xstate';
import { pet } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
    states: {
      idle: {
        on: {
          meet: {
            target: 'goodBoy',
            actions: [pet]
          }
        }
      },
      goodBoy: {
        type: 'final'
      }
    }
  });
}
