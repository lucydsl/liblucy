import { Machine, assign } from 'xstate';
import { pet } from './util';

export default Machine({
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
