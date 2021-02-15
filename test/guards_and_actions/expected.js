import { Machine } from 'https://cdn.skypack.dev/xstate';
import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js';

export default Machine({
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
