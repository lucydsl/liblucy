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
        machine.state = 'red';
      }
    },
    red: {
      timer() {
        machine.state = 'green';
      }
    }
  };
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

export function createTwo({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    start: {
      next() {
        machine.state = 'end';
        machine.done = true;
      }
    },
    end: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'start',
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
