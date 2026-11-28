import { inputValue } from './dom.js';

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

export function createEditor({ context = {} } = {}) {
  // The view as HTML, from the machine's state and context now.
  function render() {
    return `<input${attr('value', machine.context.label ?? '')}>${machine.state === 'saving' ? `<p class="status"${attr('title', machine.context.label)}${attr('data-label', machine.context.label)}>Saving ${escape(machine.context.label)}<progress${attr('max', machine.context.total)}></progress></p>` : ''}`;
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    editing: {
      input(event) {
        machine.context = { ...machine.context, label: inputValue(machine.context, event) };
      },
      save() {
        machine.state = 'saving';
      }
    },
    saving: {
      done() {
        machine.state = 'editing';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'editing',
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
