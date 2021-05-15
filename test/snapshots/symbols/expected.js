import { createMachine, assign, spawn } from 'xstate';

export default function({ actions, assigns, delays, guards, services }) {
  return createMachine({
    initial: 'idle',
    states: {
      idle: {
        on: {
          next: {
            target: 'loading',
            cond: ['isValid', 'check'],
            actions: ['log']
          }
        }
      },
      loading: {
        entry: ['updateUI', 'log', 'incrementLoads'],
        invoke: {
          src: 'loadUsers',
          onDone: 'loaded'
        }
      },
      loaded: {
        after: {
          wait: {
            target: 'homescreen',
            actions: ['logger']
          }
        }
      },
      homescreen: {
        entry: [
          'spawnTodoMachine'
        ]
      }
    }
  }, {
    guards: {
      isValid: guards.checkValid,
      check: guards.check
    },
    actions: {
      logger: actions.doLog,
      log: actions.log,
      updateUI: actions.updateUI,
      incrementLoads: assign({
        count: assigns.incrementLoads
      }),
      spawnTodoMachine: assign({
        todo: spawn(services.todoMachine, 'todoMachine')
      })
    },
    delays: {
      wait: delays.wait
    },
    services: {
      loadUsers: services.loadUsers
    }
  });
}
