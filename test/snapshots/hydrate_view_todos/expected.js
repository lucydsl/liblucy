import { preventDefault, formTitle, clearForm, isChecked } from './dom.js';

// Markup, parsed once into a fragment for the machines to clone.
function template(html) {
  const element = document.createElement('template');
  element.innerHTML = html;
  return element.content;
}

// Remove what the server rendered between a <!--[--> marker and its <!--]-->,
// a list's machines or a state's part, and return the <!--]-->.
function clearRegion(start) {
  let depth = 0;
  for(let node = start.nextSibling; ; node = start.nextSibling) {
    if(node.nodeType === 8) {
      if(node.data === '[') {
        depth++;
      } else if(node.data === ']' && depth-- === 0) {
        return node;
      }
    }
    node.remove();
  }
}

// An attribute bound to a key: there for true and other values, as
// written, and gone for false, null and undefined.
function attr(element, name, value) {
  if(value == null || value === false) {
    element.removeAttribute(name);
  } else {
    element.setAttribute(name, value === true ? '' : String(value));
  }
}

let todoItemView;

export function createTodoItem({ context = {} } = {}) {
  todoItemView ??= template('<li class="todo"><input type="checkbox"><span><!></span><button type="button" aria-label="Remove">×</button></li>');
  const dom = todoItemView.cloneNode(true);
  const nodes = [...dom.childNodes];
  const li = dom.firstChild;
  const input = li.firstChild;
  const span = input.nextSibling;
  const button = span.nextSibling;
  const titleMarker = span.firstChild;
  attr(li, 'data-done', context.done);
  input.checked = !!context.done;
  input.addEventListener('change', data => machine.send({ type: 'toggle', data }));
  titleMarker.replaceWith(context.title ?? '');
  button.addEventListener('click', data => machine.send({ type: 'remove', data }));

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    active: {
      toggle(event) {
        machine.context = { ...machine.context, done: isChecked(machine.context, event) };
        attr(li, 'data-done', machine.context.done);
        input.checked = !!machine.context.done;
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
    dom,
    nodes,
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


export function createTodoList({ context = {}, hydrate } = {}) {
  const nodes = [...hydrate.childNodes];
  const section = hydrate.firstChild;
  const form = section.firstChild;
  const ul = form.nextSibling;
  const todosEnd = clearRegion(ul.firstChild);
  form.addEventListener('submit', data => machine.send({ type: 'add', data }));
  for(const actor of context.todos ?? []) {
    todosEnd.before(...actor.nodes);
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
        todosEnd.before(...machine.context.todos.at(-1).nodes);
        clearForm(machine.context, event);
        machine.state = 'ready';
      },
      removedTodo(event) {
        if(machine.context.todos?.includes(event.data)) {
          event.data.stop();
          machine.context = { ...machine.context, todos: machine.context.todos.filter(actor => actor !== event.data) };
          for(const node of event.data.nodes) {
            node.remove();
          }
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
    nodes,
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
