import { createMachine } from 'xstate';

export default function() {
  return createMachine({
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
}
