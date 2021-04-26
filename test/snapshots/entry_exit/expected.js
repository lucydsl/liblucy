import { createMachine, assign } from 'xstate';
import { logger } from './util';

export default createMachine({
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
