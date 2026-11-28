// Compiled from Lucy by lucyc.
// @ts-nocheck

import { inputValue, preventDefault, save } from './dom.js';

// A value as HTML text, or an attribute's value.
function escape(value: unknown) {
  return String(value ?? '').replace(/[&<>"]/g, c => '&#' + c.charCodeAt(0) + ';');
}

export type FormState = 'editing' | 'saving' | 'failed';
export type FormEventName = 'input' | 'submit' | 'retry';
export type FormContextKey = 'note';

export interface CreateFormOptions<TContext extends Record<FormContextKey, any>, TEvent extends { type: FormEventName }> {
  context?: TContext;
}

export interface FormMachine<TContext extends Record<FormContextKey, any>, TEvent extends { type: FormEventName }> {
  state: FormState;
  context: TContext;
  render(): string;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: FormMachine<TContext, TEvent>) => void): () => void;
  start(): FormMachine<TContext, TEvent>;
  stop(): void;
}

export function createForm<TContext extends Record<FormContextKey, any> = Record<FormContextKey, any>, TEvent extends { type: FormEventName } = { type: FormEventName }>(options?: CreateFormOptions<TContext, TEvent>): FormMachine<TContext, TEvent>;
export function createForm({ context = {} }: any = {}): any {
  // The view as HTML, from the machine's state and context now.
  function render(): string {
    return `<form class="note"><textarea></textarea><button>Save</button>${machine.state === 'saving' ? `<p class="status">Saving…</p>` : ''}${machine.state === 'failed' ? `<p class="status status--error">That didn't work. <button type="button">Try again</button></p>` : ''}<p class="preview">Draft: ${escape(machine.context.note)}</p></form>`;
  }

  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  let savingPromise: any;
  const states: Record<string, any> = {
    editing: {
      input(event: any) {
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
      },
      submit(event: any) {
        preventDefault(machine.context, event);
        enterSaving(event);
      }
    },
    saving: {},
    failed: {
      retry(event: any) {
        enterSaving(event);
      },
      input(event: any) {
        machine.context = { ...machine.context, note: inputValue(machine.context, event) };
        machine.state = 'editing';
      }
    }
  };
  function enterSaving(event: any) {
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
  function run(step: () => void) {
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
  const machine: any = {
    state: 'editing',
    context,
    render,
    send(event: any) {
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
    subscribe(listener: any) {
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
