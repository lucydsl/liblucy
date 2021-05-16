import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      one: {

      },
      two: {

      }
    }
  });
}
