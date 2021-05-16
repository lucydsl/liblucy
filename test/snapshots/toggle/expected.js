import { createMachine } from 'xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'disabled',
    context,
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
