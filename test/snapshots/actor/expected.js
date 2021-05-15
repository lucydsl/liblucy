import { createMachine, assign, send, spawn } from 'xstate';

export function createOther() {
  return createMachine({
    states: {
      only: {
        on: {
          run: 'only'
        }
      }
    }
  });
}
export default function() {
  return createMachine({
    states: {
      idle: {
        on: {
          event: {
            target: 'idle',
            actions: [
              assign({
                first: spawn(createOther, 'other')
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
        second: spawn(createOther, 'other')
      }),
      sendThing: send('run', {
        to: (context) => context.second
      })
    }
  });
}
