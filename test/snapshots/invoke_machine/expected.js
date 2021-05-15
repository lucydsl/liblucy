import { createMachine } from 'xstate';

export function createMinute() {
  return createMachine({
    initial: 'active',
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

export function createParent() {
  return createMachine({
    initial: 'pending',
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
