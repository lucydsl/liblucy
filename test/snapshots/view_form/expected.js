import { inputValue, preventDefault, save } from './dom.js';

// Markup, parsed once into a fragment for the machines to clone.
function template(html) {
  const element = document.createElement('template');
  element.innerHTML = html;
  return element.content;
}

let formView;
let formSavingStatusView;
let formFailedStatusView;

export function createForm({ context = {} } = {}) {
  formView ??= template('<form class="note"><textarea></textarea><button>Save</button><!><p class="preview">Draft: <!></p></form>');
  const dom = formView.cloneNode(true);
  const nodes = [...dom.childNodes];
  const form = dom.firstChild;
  const textarea = form.firstChild;
  const statusSlot = textarea.nextSibling.nextSibling;
  const p = statusSlot.nextSibling;
  const noteMarker = p.firstChild.nextSibling;
  form.addEventListener('submit', data => machine.send({ type: 'submit', data }));
  textarea.addEventListener('input', data => machine.send({ type: 'input', data }));
  const noteText = document.createTextNode(context.note ?? '');
  noteMarker.replaceWith(noteText);
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
      savingPromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
