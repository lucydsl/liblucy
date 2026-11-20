import { describe } from './util.js';

export function createTodoItem({ assigns, context = {}, parent } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    active: {
      rename(event) {
        machine.context.list.send({ type: 'renamed', data: event.data });
        machine.state = 'active';
      },
      touch() {
        parent?.send({ type: 'touched', data: machine.context.id });
        machine.state = 'active';
      },
      report(event) {
        parent?.send({ type: 'report', data: describe(machine.context, event) });
        parent?.send({ type: 'summary', data: assigns.summarise(machine.context, event) });
        machine.state = 'active';
      },
      forward(event) {
        parent?.send({ type: 'forwarded', data: event.data });
        machine.state = 'active';
      },
      remove() {
        machine.context.list.send({ type: 'removed', data: machine });
        enterGone();
      }
    },
    gone: {}
  };
  function enterGone() {
    machine.state = 'gone';
    machine.done = true;
    parent?.send({ type: 'gone', data: machine });
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
