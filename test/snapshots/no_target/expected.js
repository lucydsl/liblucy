import { createMachine, assign } from 'xstate';

export default function() {
  return createMachine({
    initial: 'initial',
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
