export function createOther({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {
      run() {
        machine.state = 'only';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'only',
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

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {
      event() {
        machine.context = { ...machine.context, first: createOther().start() };
        machine.state = 'idle';
      },
      another() {
        machine.context = { ...machine.context, second: createOther().start() };
        machine.state = 'idle';
      }
    },
    end: {
      click(event) {
        machine.context.first.send({ type: 'run', data: event.data });
        machine.state = 'end';
      },
      dblclick(event) {
        machine.context.second.send({ type: 'run', data: event.data });
        machine.state = 'end';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'idle',
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
      if(machine.context.first) {
        machine.context.first.stop();
      }
      if(machine.context.second) {
        machine.context.second.stop();
      }
      listeners.clear();
    }
  };
  return machine;
}
