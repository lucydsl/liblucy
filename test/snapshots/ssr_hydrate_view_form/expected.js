import { inputValue, preventDefault, save } from './dom.js';

// A value as HTML text, or an attribute's value.
function escape(value) {
  return String(value ?? '').replace(/[&<>"]/g, c => '&#' + c.charCodeAt(0) + ';');
}

export function createForm({ context = {} } = {}) {
  // The view as HTML, from the machine's state and context now.
  function render() {
    return `<form class="note"><textarea></textarea><button>Save</button><!--[-->${machine.state === 'saving' ? `<p class="status">Saving…</p>` : ''}${machine.state === 'failed' ? `<p class="status status--error">That didn't work. <button type="button">Try again</button></p>` : ''}<!--]--><p class="preview">Draft: <!--[-->${escape(machine.context.note)}<!--]--></p></form>`;
  }

  const listeners = new Set();
  let busy = false;
  const queue = [];
  let savingPromise;
  const states = {
    editing: {
      input(event) {
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
      },
      submit(event) {
        preventDefault(machine.context, event);
        enterSaving(event);
      }
    },
    saving: {},
    failed: {
      retry(event) {
        enterSaving(event);
      },
      input(event) {
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
        machine.state = 'editing';
      }
    }
  };
  function enterSaving(event) {
    machine.state = 'saving';
    const promise = save(machine.context, event);
    savingPromise = promise;
    promise.then(() => {
      if(savingPromise !== promise) return;
      run(() => {
        machine.state = 'editing';
      });
    }, () => {
      if(savingPromise !== promise) return;
      run(() => {
        machine.state = 'failed';
      });
    });
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
      savingPromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
