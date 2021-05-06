import { createMachine } from 'xstate';

export default createMachine({
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
