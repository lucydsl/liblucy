export default function createMachine({ context = {}, services } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    init: {},
    idle: {}
  };
  function enterInit() {
    machine.state = 'init';
    machine.context = { ...machine.context, mac: services.external().start() };
    machine.state = 'idle';
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
    state: 'init',
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
      run(enterInit);
      return machine;
    },
    stop() {
      if(machine.context.mac) {
        machine.context.mac.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
