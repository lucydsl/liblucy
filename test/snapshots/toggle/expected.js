import { createMachine } from 'xstate';

export default createMachine({
  initial: 'disabled',
  states: {
    enabled: {
      on: {
        toggle: 'disabled'
      }
    },
    disabled: {
      on: {
        toggle: 'enabled'
      }
    }
  }
});
