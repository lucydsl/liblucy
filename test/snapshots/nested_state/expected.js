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
        },
        initial: 'walk',
        context,
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
