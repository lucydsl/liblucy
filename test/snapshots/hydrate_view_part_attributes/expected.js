import { inputValue } from './dom.js';

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

let editorSavingStatusView;

export function createEditor({ context = {}, hydrate } = {}) {
  const nodes = [...hydrate.childNodes];
  const input = hydrate.firstChild;
  const statusSlot = clearRegion(input.nextSibling);
  input.addEventListener('input', data => machine.send({ type: 'input', data }));
  let p;
  let labelText;
  let savingStatus;
  function makeSavingStatus() {
    editorSavingStatusView ??= template('<p class="status">Saving <!><progress></progress></p>');
    const part = editorSavingStatusView.cloneNode(true);
    p = part.firstChild;
    const labelMarker = p.firstChild.nextSibling;
    const progress = labelMarker.nextSibling;
    attr(p, 'title', machine.context.label);
    attr(p, 'data-label', machine.context.label);
    labelText = document.createTextNode(machine.context.label ?? '');
    labelMarker.replaceWith(labelText);
    attr(progress, 'max', machine.context.total);
    return [...part.childNodes];
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    editing: {
      input(event) {
        machine.context = { ...machine.context, label: inputValue(machine.context, event) };
        input.value = machine.context.label ?? '';
        if(labelText) {
          labelText.data = machine.context.label ?? '';
        }
        if(p) {
          attr(p, 'title', machine.context.label);
          attr(p, 'data-label', machine.context.label);
        }
      },
      save() {
        enterSaving();
      }
    },
    saving: {
      done() {
        exitSaving();
        machine.state = 'editing';
      }
    }
  };
  function enterSaving() {
    machine.state = 'saving';
    savingStatus ??= makeSavingStatus();
    statusSlot.before(...savingStatus);
  }
  function exitSaving() {
    for(const node of savingStatus) {
      node.remove();
    }
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'editing',
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
