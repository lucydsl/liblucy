export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {
      purchase() {
        machine.state = 'end';
        machine.done = true;
      },
      delay() {
        machine.state = 'end';
        machine.done = true;
      },
      SNAKE_CASE() {
        machine.state = 'end';
        machine.done = true;
      },
      ANOTHER_SNAKE() {
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
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
