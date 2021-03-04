import { Machine } from 'xstate';
import { isDog } from './util';

export default Machine({
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
