import { createMachine } from 'xstate';

export function createMinute({ context = {} } = {}) {
  return createMachine({
    initial: 'active',
    context,
    states: {
      active: {
        on: {
          timer: 'finished'
        }
      },
      finished: {
        type: 'final'
      }
    }
  });
}

export function createParent({ context = {} } = {}) {
  return createMachine({
    initial: 'pending',
    context,
    states: {
      pending: {
        invoke: {
          src: createMinute,
          onDone: 'timesUp'
        }
      },
      timesUp: {
        type: 'final'
      }
    }
  });
}
