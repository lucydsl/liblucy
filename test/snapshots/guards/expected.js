import { isValidCard, isInvalidCard, moneyWithdrawn } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    idle: {
      next(event) {
        if(isValidCard(machine.context, event)) {
          enterPurchase(event);
        } else if(isInvalidCard(machine.context, event)) {
          machine.state = 'error';
          machine.done = true;
        }
      }
    },
    purchase: {},
    purchased: {},
    error: {}
  };
  function enterPurchase(event) {
    machine.state = 'purchase';
    if(moneyWithdrawn(machine.context, event)) {
      machine.state = 'purchased';
      machine.done = true;
    }
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'idle',
    context,
    done: false,
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
