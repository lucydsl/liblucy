import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
    states: {
      idle: {
        on: {
          fetch: 'fetching'
        },
        initial: 'noError',
        context,
        states: {
          noError: {

          },
          errored: {

          }
        }
      },
      fetching: {
        on: {
          reportError: 'idle.errored'
        }
      }
    }
  });
}
