export default function createMachine({ context = {}, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
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
    state: 'one',
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
      run(() => enterOne({ type: 'start' }));
      return machine;
    },
    stop() {
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
