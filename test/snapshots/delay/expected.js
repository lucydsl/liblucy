import { calcLightDelay } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let greenTimer;
  let yellowTimer;
  let redTimer;
  const states = {
    green: {},
    yellow: {
      go() {
        exitYellow();
        enterGreen();
      }
    },
    red: {}
  };
  function enterGreen() {
    machine.state = 'green';
    greenTimer = setTimeout(() => {
      run(() => {
        enterYellow();
      });
    }, 1000);
  }
  function enterYellow() {
    machine.state = 'yellow';
    yellowTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        exitYellow();
        enterRed(event);
      });
    }, 500);
  }
  function exitYellow() {
    clearTimeout(yellowTimer);
  }
  function enterRed(event) {
    machine.state = 'red';
    redTimer = setTimeout(() => {
      run(() => {
        enterGreen();
      });
    }, calcLightDelay(machine.context, event));
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
    state: 'green',
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
      run(enterGreen);
      return machine;
    },
    stop() {
      clearTimeout(greenTimer);
      clearTimeout(yellowTimer);
      clearTimeout(redTimer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
