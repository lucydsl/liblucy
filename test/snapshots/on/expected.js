import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'idle',
    context,
    states: {
      idle: {
        on: {
          purchase: 'end',
          delay: 'end',
          SNAKE_CASE: 'end',
          ANOTHER_SNAKE: 'end'
        }
      },
      end: {
        type: 'final'
      }
    }
  });
}
