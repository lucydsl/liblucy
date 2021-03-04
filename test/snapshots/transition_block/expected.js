import { Machine } from 'xstate';
import { check } from './stuff.js';

export default Machine({
  initial: 'start',
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
