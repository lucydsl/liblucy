Event [pong] in state [only] is not declared by machine [kid].
Event [unused] is declared by machine [kid], but none of its states handles it.
send(child, nope) in state [idle] sends an event that machine [kid] does not declare.
assign(last) in state [idle] stores the data of event [stop], which is declared without any.
export function createKid({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {
      ping() {
        machine.state = 'only';
      },
      pong() {
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

export function createParent({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    idle: {
      go(event) {
        machine.context.child.send({ type: 'ping', data: event.data });
        machine.context.child.send({ type: 'nope', data: event.data });
        enterIdle();
      },
      stop(event) {
        machine.context = { ...machine.context, last: event.data };
        enterIdle();
      }
    }
  };
  function enterIdle() {
    machine.state = 'idle';
    machine.context = { ...machine.context, child: createKid().start() };
  }
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
      enterIdle();
      notify();
      return machine;
    },
    stop() {
      if(machine.context.child) {
        machine.context.child.stop();
      }
      listeners.clear();
    }
  };
  return machine;
}
