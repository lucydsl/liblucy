import { log, loadSub } from './util.js';

export function createSubtask({ context = {}, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    open: {
      close(event) {
        if(guards.canClose(machine.context, event)) {
          machine.state = 'closed';
          machine.done = true;
        }
      }
    },
    closed: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'open',
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

export function createTodoItem({ actions, assigns, context = {}, delays, guards, services } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let activeTimer;
  let removedPromise;
  const states = {
    active: {
      remove(event) {
        if(guards.canDelete(machine.context, event)) {
          exitActive();
          actions.log(machine.context, event);
          machine.context = { ...machine.context, reason: assigns.why(machine.context, event) };
          enterRemoved(event);
        }
      },
      split(event) {
        exitActive();
        machine.context = {
          ...machine.context,
          subtasks: [
            ...(machine.context.subtasks ?? []),
            createSubtask({
              guards: {
                canClose: guards.canCloseSub
              }
            }).start()
          ]
        };
        enterActive(event);
      }
    },
    removed: {}
  };
  function enterActive(event) {
    machine.state = 'active';
    activeTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        exitActive();
        enterActive(event);
      });
    }, delays.wait(machine.context, event));
  }
  function exitActive() {
    clearTimeout(activeTimer);
  }
  function enterRemoved(event) {
    machine.state = 'removed';
    const promise = services.save(machine.context, event);
    removedPromise = promise;
    promise.then(data => {
      if(removedPromise !== promise) return;
      const event = { type: 'done', data };
      run(() => {
        enterActive(event);
      });
    });
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
    state: 'active',
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
      run(() => enterActive({ type: 'start' }));
      return machine;
    },
    stop() {
      clearTimeout(activeTimer);
      removedPromise = undefined;
      for(const actor of machine.context.subtasks ?? []) {
        actor.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createTodoList({ assigns, context = {}, delays, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    ready: {
      add(event) {
        if(guards.canAdd(machine.context, event)) {
          machine.context = {
            ...machine.context,
            todos: [
              ...(machine.context.todos ?? []),
              createTodoItem({
                context: {
                  title: event.data
                },
                actions: {
                  log
                },
                assigns: {
                  why: assigns.reasonFor
                },
                delays: {
                  wait: delays.todoWait
                },
                guards: {
                  canDelete: guards.canDeleteTodo,
                  canCloseSub: guards.canAdd
                },
                services: {
                  save: loadSub
                }
              }).start()
            ]
          };
          machine.state = 'ready';
        }
      },
      only() {
        machine.context.one?.stop();
        machine.context = {
          ...machine.context,
          one: createSubtask({
            guards: {
              canClose: guards.canAdd
            }
          }).start()
        };
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
      for(const actor of machine.context.todos ?? []) {
        actor.stop();
      }
      if(machine.context.one) {
        machine.context.one.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
