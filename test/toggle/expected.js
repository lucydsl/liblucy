import { Machine } from 'https://cdn.skypack.dev/xstate';

export default Machine({
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
