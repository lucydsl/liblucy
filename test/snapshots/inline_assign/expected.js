import { pet } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
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
      run(() => {
        machine.context = { ...machine.context, wilbur: event.data };
        machine.state = 'goodBoy';
        machine.done = true;
      });
    });
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  function run(step) {
    busy = true;
    try {
      step();
      notify();
      busy = false;
      if(queue.length) {
        machine.send(queue.shift());
      }
    } finally {
      queue.length = 0;
      busy = false;
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
      run(() => enterIdle({ type: 'start' }));
      return machine;
    },
    stop() {
      idlePromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
