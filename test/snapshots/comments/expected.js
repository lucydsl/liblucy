import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      one: {
        on: {
          go: 'another'
        }
      },
      another: {
        on: {
          last: 'third'
        }
      },
      third: {

      }
    }
  });
}
