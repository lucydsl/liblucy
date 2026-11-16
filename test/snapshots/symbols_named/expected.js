export function createNamed({ actions, assigns, context = {}, delays, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let busyWaitingTimer;
  const states = {
    idle: {
      go(event) {
        if(guards.ok(machine.context, event)) {
          actions.log(machine.context, event);
          machine.context = { ...machine.context, n: assigns.next(machine.context, event) };
          enterBusy(event);
        }
      }
    },
    'busy.waiting': {},
    'busy.done': {}
  };
  function enterBusy(event) {
    enterBusyWaiting(event);
  }
  function enterBusyWaiting(event) {
    machine.state = 'busy.waiting';
    actions.enter(machine.context, event);
    busyWaitingTimer = setTimeout(() => {
      run(() => {
        machine.state = 'busy.done';
      });
    }, delays.wait(machine.context, event));
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
    state: 'idle',
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
      notify();
      return machine;
    },
    stop() {
      clearTimeout(busyWaitingTimer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
