import { createMachine } from 'xstate';

export default function() {
  return createMachine({
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
