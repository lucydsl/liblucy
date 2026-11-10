import { isDog } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {
      pet(event) {
        if(isDog(machine.context, event)) {
          enterPet();
        }
      }
    },
    pet: {},
    goodBoy: {}
  };
  function enterPet() {
    machine.state = 'pet';
    machine.state = 'goodBoy';
    machine.done = true;
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
