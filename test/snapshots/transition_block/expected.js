import { createMachine } from 'xstate';
import { check } from './stuff.js';

export default function({ context = {} } = {}) {
  return createMachine({
    initial: 'start',
    context,
    states: {
      start: {
        on: {
          go: {
            target: 'end',
            cond: ['canGo', 'sureCanGo', 'AreWeReallySure']
          }
        }
      },
      end: {
        type: 'final'
      }
    }
  }, {
    guards: {
      canGo: check,
      sureCanGo: check,
      AreWeReallySure: check
    }
  });
}
