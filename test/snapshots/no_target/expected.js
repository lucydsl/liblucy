import { createMachine, assign } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'initial',
    context,
    states: {
      initial: {
        on: {
          test: {
            actions: [
              assign({
                someValue: (context, event) => event.data
              })
            ]
          }
        }
      }
    }
  });
}
