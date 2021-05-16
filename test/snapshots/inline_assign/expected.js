import { createMachine, assign } from 'xstate';
import { pet } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
    states: {
      idle: {
        invoke: {
          src: pet,
          onDone: {
            target: 'goodBoy',
            actions: [
              assign({
                wilbur: (context, event) => event.data
              })
            ]
          }
        }
      },
      goodBoy: {
        type: 'final'
      }
    }
  });
}
