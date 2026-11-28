import { inputValue } from './dom.js';

// The text node the server rendered for a key, after its <!--[--> marker,
// or an empty one put there if its text was empty.
function claimText(start) {
  let text = start.nextSibling;
  if(text.nodeType !== 3) {
    text = document.createTextNode('');
    start.after(text);
  }
  return text;
}


export function createGreeter({ context = {}, hydrate } = {}) {
  const nodes = [...hydrate.childNodes];
  const span = hydrate.firstChild;
  const input = span.nextSibling;
  const nameText = claimText(span.firstChild.nextSibling);
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
