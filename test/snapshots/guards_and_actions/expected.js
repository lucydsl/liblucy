import { createMachine, assign } from 'xstate';
import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js';

export default function() {
  return createMachine({
    initial: 'active',
    states: {
      active: {
        on: {
          inc: {
            cond: 'isNotMax',
            actions: ['increment']
          },
          dec: {
            cond: 'isNotMin',
            actions: ['decrement']
          }
        }
      }
    }
  }, {
    guards: {
      isNotMax: lessThanTen,
      isNotMin: greaterThanZero
    },
    actions: {
      increment: assign({
        count: incrementCount
      }),
      decrement: assign({
        count: decrementCount
      })
    }
  });
}
