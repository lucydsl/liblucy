import { inputValue } from './dom.js';

// Markup, parsed once into a fragment for the machines to clone.
function template(html) {
  const element = document.createElement('template');
  element.innerHTML = html;
  return element.content;
}

let greeterView;

export function createGreeter({ context = {} } = {}) {
  greeterView ??= template('<span class="greeting">Name: <!></span><input type="text">');
  const dom = greeterView.cloneNode(true);
  const nodes = [...dom.childNodes];
  const span = dom.firstChild;
  const input = span.nextSibling;
  const nameMarker = span.firstChild.nextSibling;
  const nameText = document.createTextNode(context.name ?? '');
  nameMarker.replaceWith(nameText);
  input.value = context.name ?? '';
  input.addEventListener('input', data => machine.send({ type: 'input', data }));

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    idle: {
      input(event) {
        machine.context = { ...machine.context, name: inputValue(machine.context, event) };
        nameText.data = machine.context.name ?? '';
        input.value = machine.context.name ?? '';
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
    dom,
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
