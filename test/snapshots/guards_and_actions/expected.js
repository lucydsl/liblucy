import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    active: {
      inc(event) {
        if(lessThanTen(machine.context, event)) {
          machine.context = { ...machine.context, count: incrementCount(machine.context, event) };
        }
      },
      dec(event) {
        if(greaterThanZero(machine.context, event)) {
          machine.context = { ...machine.context, count: decrementCount(machine.context, event) };
        }
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'active',
    context,
    send(event) {
      if(typeof event === 'string') {
        event = { type: event };
      }
      const handler = states[machine.state][event.type];
      if(handler) {
        handler(event);
        notify();
      }
    },
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
