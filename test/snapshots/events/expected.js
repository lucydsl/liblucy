import { isAdmin } from './util';

export function createCounter({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
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
