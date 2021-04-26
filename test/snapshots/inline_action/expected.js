import { createMachine } from 'xstate';
import { pet } from './util';

export default createMachine({
  initial: 'idle',
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
