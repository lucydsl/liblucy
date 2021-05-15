import { createMachine } from 'xstate';

export default function() {
  return createMachine({
    initial: 'one',
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
