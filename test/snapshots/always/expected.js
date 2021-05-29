import { createMachine } from 'xstate';

export default function({ context = {}, guards } = {}) {
  return createMachine({
    initial: 'one',
    context,
    states: {
      one: {
        always: [
          {
            target: 'two',
            cond: 'canGo'
          }, {
            target: 'two'
          }
        ]
      },
      two: {
        type: 'final'
      }
    }
  }, {
    guards: {
      canGo: guards.canGo
    }
  });
}
