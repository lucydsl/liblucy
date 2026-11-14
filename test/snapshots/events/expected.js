import { isAdmin } from './util';

export function createCounter({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {
      set(event) {
        machine.context = { ...machine.context, count: event.data };
      },
      login(event) {
        if(isAdmin(machine.context, event)) {
          machine.context = { ...machine.context, user: event.data };
          machine.state = 'active';
        }
      },
      reset() {
        machine.state = 'idle';
      }
    },
    active: {
      reset() {
        machine.state = 'idle';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'idle',
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
