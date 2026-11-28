import { preventDefault, formTitle, clearForm, isChecked } from './dom.js';

// A value as HTML text, or an attribute's value.
function escape(value) {
  return String(value ?? '').replace(/[&<>"]/g, c => '&#' + c.charCodeAt(0) + ';');
}

// An attribute bound to a key: there for true and other values, as
// written, and left out for false, null and undefined.
function attr(name, value) {
  if(value == null || value === false) {
    return '';
  }
  return value === true ? ' ' + name : ' ' + name + '="' + escape(value) + '"';
}

export function createTodoItem({ context = {} } = {}) {
  // The view as HTML, from the machine's state and context now.
  function render() {
    return `<li class="todo"${attr('data-done', machine.context.done)}><input type="checkbox"${attr('checked', !!machine.context.done)}><span><!--[-->${escape(machine.context.title)}<!--]--></span><button type="button" aria-label="Remove">×</button></li>`;
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    active: {
      toggle(event) {
        machine.context = { ...machine.context, done: isChecked(machine.context, event) };
      },
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
    render,
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

export function createTodoList({ context = {} } = {}) {
  // The view as HTML, from the machine's state and context now.
  function render() {
    return `<section class="todos"><form><input name="title" placeholder="What needs doing?" autocomplete="off"><button>Add</button></form><ul><!--[-->${(machine.context.todos ?? []).map(actor => actor.render()).join('')}<!--]--></ul></section>`;
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    ready: {
      add(event) {
        preventDefault(machine.context, event);
        machine.context = {
          ...machine.context,
          todos: [
            ...(machine.context.todos ?? []),
            createTodoItem({
              context: {
                title: formTitle(machine.context, event),
                list: machine
              }
            }).start()
          ]
        };
        clearForm(machine.context, event);
        machine.state = 'ready';
      },
      removedTodo(event) {
        if(machine.context.todos?.includes(event.data)) {
          event.data.stop();
          machine.context = { ...machine.context, todos: machine.context.todos.filter(actor => actor !== event.data) };
        }
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
    render,
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
