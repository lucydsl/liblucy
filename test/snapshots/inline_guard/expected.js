import { createMachine } from 'xstate';
import { isDog } from './util';

export default createMachine({
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
