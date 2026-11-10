import { check } from './stuff.js';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    start: {
      go(event) {
        if(check(machine.context, event) && check(machine.context, event) && check(machine.context, event)) {
          machine.state = 'end';
          machine.done = true;
        }
      }
    },
    end: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'start',
    context,
    done: false,
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
