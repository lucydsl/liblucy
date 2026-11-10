export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    'idle.noError': {
      fetch: idleOnFetch
    },
    'idle.errored': {
      fetch: idleOnFetch
    },
    fetching: {
      reportError() {
        machine.state = 'idle.errored';
      }
    }
  };
  function idleOnFetch() {
    machine.state = 'fetching';
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'idle.noError',
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
