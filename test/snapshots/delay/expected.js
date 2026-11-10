import { calcLightDelay } from './util';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
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
      enterYellow();
      notify();
    }, 1000);
  }
  function enterYellow() {
    machine.state = 'yellow';
    yellowTimer = setTimeout(() => {
      const event = { type: 'delay' };
      exitYellow();
      enterRed(event);
      notify();
    }, 500);
  }
  function exitYellow() {
    clearTimeout(yellowTimer);
  }
  function enterRed(event) {
    machine.state = 'red';
    redTimer = setTimeout(() => {
      enterGreen();
      notify();
    }, calcLightDelay(machine.context, event));
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'green',
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
      enterGreen();
      notify();
      return machine;
    },
    stop() {
      clearTimeout(greenTimer);
      clearTimeout(yellowTimer);
      clearTimeout(redTimer);
      listeners.clear();
    }
  };
  return machine;
}
