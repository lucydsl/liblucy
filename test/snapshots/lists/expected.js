export function createTodoItem({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    active: {
      remove() {
        enterRemoved();
      }
    },
    removed: {}
  };
  function enterRemoved() {
    machine.state = 'removed';
    machine.done = true;
    machine.context.list.send({ type: 'removedTodo', data: machine });
  }
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createPlain({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {}
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

export function createTodoList({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    ready: {
      add(event) {
        machine.context = {
          ...machine.context,
          todos: [
            ...(machine.context.todos ?? []),
            createTodoItem({
              context: {
                title: event.data,
                list: machine
              }
            }).start()
          ]
        };
        enterReady();
      },
      removedTodo(event) {
        if(machine.context.todos?.includes(event.data)) {
          event.data.stop();
          machine.context = { ...machine.context, todos: machine.context.todos.filter(actor => actor !== event.data) };
        }
        enterReady();
      },
      drop() {
        if(machine.context.others?.includes(machine.context.selected)) {
          machine.context.selected.stop();
          machine.context = { ...machine.context, others: machine.context.others.filter(actor => actor !== machine.context.selected) };
        }
        enterReady();
      }
    }
  };
  function enterReady() {
    machine.state = 'ready';
    machine.context = {
      ...machine.context,
      others: [
        ...(machine.context.others ?? []),
        createPlain().start()
      ]
    };
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
    state: 'ready',
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
      run(enterReady);
      return machine;
    },
    stop() {
      for(const actor of machine.context.others ?? []) {
        actor.stop();
      }
      for(const actor of machine.context.todos ?? []) {
        actor.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
