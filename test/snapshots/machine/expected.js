import { createMachine } from 'xstate';

export function createLight({ context = {} } = {}) {
  return createMachine({
    initial: 'green',
    context,
    states: {
      green: {
        on: {
          timer: 'yellow'
        }
      },
      yellow: {
        on: {
          timer: 'red'
        }
      },
      red: {
        on: {
          timer: 'green'
        }
      }
    }
  });
}

export function createTwo({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      start: {
        on: {
          next: 'end'
        }
      },
      end: {
        type: 'final'
      }
    }
  });
}
