import { createMachine, assign, send, spawn } from 'xstate';

export const other = createMachine({
  states: {
    only: {
      on: {
        run: 'only'
      }
    }
  }
});
export default createMachine({
  states: {
    idle: {
      on: {
        event: {
          target: 'idle',
          actions: [
            assign({
              first: spawn(other, 'other')
            })
          ]
        },
        another: {
          target: 'idle',
          actions: ['makeThing']
        }
      }
    },
    end: {
      on: {
        click: {
          target: 'end',
          actions: [
            send('run', {
              to: (context) => context.first
            })
          ]
        },
        dblclick: {
          target: 'end',
          actions: ['sendThing']
        }
      }
    }
  }
}, {
  actions: {
    makeThing: assign({
      second: spawn(other, 'other')
    }),
    sendThing: send('run', {
      to: (context) => context.second
    })
  }
});
