import { incrementCount, decrementCount, lessThanTen, greaterThanZero } from './actions.js';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
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
      if(busy) {
        queue.push(event);
        return;
      }
      busy = true;
      try {
        do {
          const handler = states[machine.state][event.type];
          if(handler) {
            handler(event);
            notify();
          }
        } while((event = queue.shift()));
      } finally {
        queue.length = 0;
        busy = false;
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
