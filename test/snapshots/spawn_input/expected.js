import { makeId } from './util.js';

export function createTodoItem({ context = {}, parent } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    active: {
      remove(event) {
        enterRemoved(event);
      }
    },
    removed: {}
  };
  function enterRemoved(event) {
    machine.state = 'removed';
    machine.done = true;
    parent?.send({ type: 'removedTodo', data: event.data });
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

export function createTodoList({ assigns, context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    ready: {
      add(event) {
        machine.context.todo?.stop();
        machine.context = {
          ...machine.context,
          todo: createTodoItem({
            context: {
              title: event.data,
              storage: machine.context.storage,
              id: machine
            },
            parent: machine
          }).start()
        };
        machine.state = 'ready';
      },
      draft(event) {
        machine.context.draft?.stop();
        machine.context = {
          ...machine.context,
          draft: createTodoItem({
            context: {
              title: assigns.draftTitle(machine.context, event),
              storage: machine.context.storage,
              id: makeId(machine.context, event)
            },
            parent: machine
          }).start()
        };
        machine.state = 'ready';
      },
      removedTodo() {
        machine.state = 'ready';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
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
      notify();
      return machine;
    },
    stop() {
      if(machine.context.todo) {
        machine.context.todo.stop();
      }
      if(machine.context.draft) {
        machine.context.draft.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
