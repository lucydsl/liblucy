import { log2, isBase64 } from './util.js';

export function createStep2({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let s2Timer;
  const states = {
    s1: {
      go2(event) {
        if(isBase64(machine.context, event)) {
          enterS2();
        }
      }
    },
    s2: {
      back2s1(event) {
        exitS2();
        machine.context = { ...machine.context, last2: event.data };
        enterS1(event);
      }
    },
    step_3: {}
  };
  function enterS1(event) {
    machine.state = 's1';
    log2(machine.context, event);
  }
  function enterS2() {
    machine.state = 's2';
    s2Timer = setTimeout(() => {
      run(() => {
        exitS2();
        machine.state = 'step_3';
        machine.done = true;
      });
    }, 2000);
  }
  function exitS2() {
    clearTimeout(s2Timer);
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
    state: 's1',
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
      run(() => enterS1({ type: 'start' }));
      return machine;
    },
    stop() {
      clearTimeout(s2Timer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
