import { createMachine } from 'https://cdn.skypack.dev/xstate';

export default function({ context = {} } = {}) {
  return createMachine({
    context,
    states: {
      idle: {

      }
    }
  });
}