export default function createMachine({ context = {}, guards } = {}) {
  const listeners = new Set();
  const states = {
    one: {},
    two: {}
  };
  function enterOne(event) {
    machine.state = 'one';
    if(guards.canGo(machine.context, event)) {
      machine.state = 'two';
      machine.done = true;
    } else {
      machine.state = 'two';
      machine.done = true;
    }
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'one',
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
      enterOne({ type: 'start' });
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
