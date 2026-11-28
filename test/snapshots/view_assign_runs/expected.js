import { sample, elapsedText, elapsedClass, queryText } from './query.js';

// Markup, parsed once into a fragment for the machines to clone.
function template(html) {
  const element = document.createElement('template');
  element.innerHTML = html;
  return element.content;
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

let queryView;

export function createQuery({ context = {} } = {}) {
  queryView ??= template('<td><!><span><!></span></td>');
  const dom = queryView.cloneNode(true);
  const nodes = [...dom.childNodes];
  const td = dom.firstChild;
  const elapsedMarker = td.firstChild;
  const span = elapsedMarker.nextSibling;
  const textMarker = span.firstChild;
  attr(td, 'class', context.className);
  const elapsedText2 = document.createTextNode(context.elapsed ?? '');
  elapsedMarker.replaceWith(elapsedText2);
  const textText = document.createTextNode(context.text ?? '');
  textMarker.replaceWith(textText);

  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    live: {
      frame(event) {
        {
          const context = { ...machine.context };
          context.sample = sample(context, event);
          context.elapsed = elapsedText(context, event);
          context.className = elapsedClass(context, event);
          context.text = queryText(context, event);
          context.elapsed = elapsedText(context, event);
          machine.context = context;
        }
        elapsedText2.data = machine.context.elapsed ?? '';
        attr(td, 'class', machine.context.className);
        textText.data = machine.context.text ?? '';
      },
      reset(event) {
        machine.context = { ...machine.context, elapsed: event.data, text: event.data };
        elapsedText2.data = machine.context.elapsed ?? '';
        textText.data = machine.context.text ?? '';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'live',
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
