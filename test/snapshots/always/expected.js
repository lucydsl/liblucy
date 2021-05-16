import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'one',
    context,
    states: {
      one: {
        always: [
          {
            target: 'two'
          }
        ]
      },
      two: {
        type: 'final'
      }
    }
  });
}
