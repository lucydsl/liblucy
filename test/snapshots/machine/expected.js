import { createMachine } from 'xstate';

export function createLight() {
  return createMachine({
    initial: 'green',
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

export function createTwo() {
  return createMachine({
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
