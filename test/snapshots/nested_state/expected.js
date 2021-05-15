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
        },
        initial: 'walk',
        states: {
          walk: {
            on: {
              countdown: 'wait'
            }
          },
          wait: {
            on: {
              countdown: 'stop'
            }
          },
          stop: {
            type: 'final'
          }
        }
      },
      another: {

      }
    }
  });
}
