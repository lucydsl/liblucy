import { createMachine, assign } from 'xstate';
import { getUser, setUser } from './user.js';

export default function({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      loading: {
        on: {
          again: 'loading'
        },
        invoke: {
          src: getUser,
          onDone: {
            target: 'ready',
            actions: ['assignUser']
          },
          onError: 'error'
        }
      },
      ready: {

      },
      error: {

      }
    }
  }, {
    actions: {
      assignUser: assign({
        user: setUser
      })
    }
  });
}
