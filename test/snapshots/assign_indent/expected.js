import { createMachine, assign } from 'xstate';
import { logger } from './util';

export default function({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      idle: {
        on: {
          go: {
            actions: [
              assign({
                prop: (context, event) => event.data
              }), 
              'log'
            ]
          }
        }
      }
    }
  }, {
    actions: {
      log: logger
    }
  });
}
