import { Machine } from 'xstate';
import { pet } from './util';

export default Machine({
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
