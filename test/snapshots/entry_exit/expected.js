import { logger } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {},
    end: {}
  };
  function enterIdle(event) {
    machine.state = 'idle';
    logger(machine.context, event);
  }
  function exitIdle(event) {
    machine.context = { ...machine.context, wilbur: event.data };
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
      enterIdle({ type: 'start' });
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
