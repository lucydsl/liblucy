import { isValidCard, isInvalidCard, moneyWithdrawn } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
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
