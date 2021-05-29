import { createMachine } from 'xstate';
import { logger } from './util';

export function createOne({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      idle: {
        on: {
          go: {
            actions: ['log']
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

export function createTwo({ context = {} } = {}) {
  return createMachine({
    context
  });
}
