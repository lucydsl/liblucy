import { pet } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let idlePromise;
  const states = {
    idle: {},
    goodBoy: {}
  };
  function enterIdle(event) {
    machine.state = 'idle';
    const promise = pet(machine.context, event);
    idlePromise = promise;
    promise.then(data => {
      if(idlePromise !== promise) return;
      const event = { type: 'done', data };
      machine.context = { ...machine.context, wilbur: event.data };
      machine.state = 'goodBoy';
      machine.done = true;
      notify();
    });
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
      idlePromise = undefined;
      listeners.clear();
    }
  };
  return machine;
}
