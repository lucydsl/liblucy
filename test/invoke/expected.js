import { Machine } from 'xstate';
import { getUser, setUser } from './user.js';

export default Machine({
  states: {
    loading: {
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
