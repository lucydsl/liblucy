import { Machine } from 'xstate';

export default Machine({
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
