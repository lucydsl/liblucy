import { inputValue, preventDefault, save } from './dom.js';

// Markup, parsed once into a fragment for the machines to clone.
function template(html) {
  const element = document.createElement('template');
  element.innerHTML = html;
  return element.content;
}

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

let formSavingStatusView;
let formFailedStatusView;

export function createForm({ context = {}, hydrate } = {}) {
  const nodes = [...hydrate.childNodes];
  const form = hydrate.firstChild;
  const textarea = form.firstChild;
  const statusSlot = clearRegion(textarea.nextSibling.nextSibling);
  const p = statusSlot.nextSibling;
  const noteText = claimText(p.firstChild.nextSibling);
  form.addEventListener('submit', data => machine.send({ type: 'submit', data }));
  textarea.addEventListener('input', data => machine.send({ type: 'input', data }));
  let savingStatus;
  function makeSavingStatus() {
    formSavingStatusView ??= template('<p class="status">Saving…</p>');
    const part = formSavingStatusView.cloneNode(true);
    return [...part.childNodes];
  }
  let failedStatus;
  function makeFailedStatus() {
    formFailedStatusView ??= template('<p class="status status--error">That didn\'t work. <button type="button">Try again</button></p>');
    const part = formFailedStatusView.cloneNode(true);
    const p2 = part.firstChild;
    const button = p2.firstChild.nextSibling;
    button.addEventListener('click', data => machine.send({ type: 'retry', data }));
    return [...part.childNodes];
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  let savingPromise;
  const states = {
    editing: {
      input(event) {
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
        noteText.data = machine.context.note ?? '';
      },
      submit(event) {
        preventDefault(machine.context, event);
        enterSaving(event);
      }
    },
    saving: {},
    failed: {
      retry(event) {
        exitFailed();
        enterSaving(event);
      },
      input(event) {
        exitFailed();
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
        noteText.data = machine.context.note ?? '';
        machine.state = 'editing';
      }
    }
  };
  function enterSaving(event) {
    machine.state = 'saving';
    savingStatus ??= makeSavingStatus();
    statusSlot.before(...savingStatus);
    const promise = save(machine.context, event);
    savingPromise = promise;
    promise.then(() => {
      if(savingPromise !== promise) return;
      run(() => {
        exitSaving();
        machine.state = 'editing';
      });
    }, () => {
      if(savingPromise !== promise) return;
      run(() => {
        exitSaving();
        enterFailed();
      });
    });
  }
  function exitSaving() {
    savingPromise = undefined;
    for(const node of savingStatus) {
      node.remove();
    }
  }
  function enterFailed() {
    machine.state = 'failed';
    failedStatus ??= makeFailedStatus();
    statusSlot.before(...failedStatus);
  }
  function exitFailed() {
    for(const node of failedStatus) {
      node.remove();
    }
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
      savingPromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
