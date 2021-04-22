import { Machine, assign } from 'xstate';
import { logger } from './util';

export default Machine({
  states: {
    idle: {
      entry: ['logSomething'],
      exit: [
        assign({
          wilbur: (context, event) => event.data
        })
      ]
    },
    end: {
      type: 'final'
    }
  }
}, {
  actions: {
    logSomething: logger
  }
});
