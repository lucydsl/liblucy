import { createMachine } from 'xstate';
import { isValidCard, isInvalidCard, moneyWithdrawn } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
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
}
