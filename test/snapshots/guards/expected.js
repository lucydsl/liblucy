import { createMachine } from 'xstate';
import { isValidCard, isInvalidCard } from './util';

export default createMachine({
  initial: 'idle',
  states: {
    idle: {
      on: {
        next: [
          {
            target: 'purchase',
            cond: isValidCard
          }, 
          {
            target: 'error',
            cond: isInvalidCard
          }
        ]
      }
    },
    purchase: {
      type: 'final'
    },
    error: {
      type: 'final'
    }
  }
});
