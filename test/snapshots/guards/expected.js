import { createMachine } from 'xstate';
import { isValidCard, isInvalidCard, moneyWithdrawn } from './util';

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
      always: [
        {
          target: 'purchased',
          cond: 'isMoneyWithdrawn'
        }
      ]
    },
    purchased: {
      type: 'final'
    },
    error: {
      type: 'final'
    }
  }
}, {
  guards: {
    isMoneyWithdrawn: moneyWithdrawn
  }
});
