export function createMinute({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    active: {
      timer() {
        machine.state = 'finished';
        machine.done = true;
      }
    },
    finished: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'active',
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

export function createParent({ context = {} } = {}) {
  const listeners = new Set();
  let pendingChild;
  const states = {
    pending: {},
    timesUp: {}
  };
  function enterPending() {
    machine.state = 'pending';
    const child = createMinute();
    pendingChild = child;
    child.subscribe(() => {
      if(child.done) {
        exitPending();
        machine.state = 'timesUp';
        machine.done = true;
        notify();
      }
    });
    child.start();
  }
  function exitPending() {
    pendingChild.stop();
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'pending',
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
      enterPending();
      notify();
      return machine;
    },
    stop() {
      if(pendingChild) {
        pendingChild.stop();
      }
      listeners.clear();
    }
  };
  return machine;
}
