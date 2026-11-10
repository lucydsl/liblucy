export function createLight({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    green: {
      timer() {
        machine.state = 'yellow';
      }
    },
    yellow: {
      timer() {
        machine.state = 'red.walk';
      }
    },
    'red.walk': {
      countdown() {
        machine.state = 'red.wait';
      },
      timer: redOnTimer
    },
    'red.wait': {
      countdown() {
        machine.state = 'red.stop';
      },
      timer: redOnTimer
    },
    'red.stop': {
      timer: redOnTimer
    },
    another: {}
  };
  function redOnTimer() {
    machine.state = 'green';
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
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
