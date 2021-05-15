import { createMachine, assign } from 'xstate';
import { pet } from './util';

export default function() {
  return createMachine({
    initial: 'idle',
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
