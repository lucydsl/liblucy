import { Machine } from 'xstate';
import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js';

export default Machine({
  initial: 'active',
  states: {
    active: {
      on: {
        inc: {
          target: 'active',
          cond: 'isNotMax',
          actions: ['increment']
        },
        dec: {
          target: 'active',
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
