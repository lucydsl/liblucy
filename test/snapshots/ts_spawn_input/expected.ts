// Compiled from Lucy by lucyc.
// @ts-nocheck

import { makeId } from './util.js';
import type { Storage } from './types';

export type TodoItemState = 'active' | 'removed';
export type TodoItemEventName = 'remove';
export interface TodoItemInput {
  title: string;
  storage: Storage;
  id: unknown;
}

export interface CreateTodoItemOptions<TContext extends Record<string, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  context: TContext;
  parent?: { send(event: { type: 'removedTodo'; data?: unknown }): void };
}

export interface TodoItemMachine<TContext extends Record<string, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  state: TodoItemState;
  context: TContext;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoItemMachine<TContext, TEvent>) => void): () => void;
  start(): TodoItemMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoItem<TContext extends Record<string, any> & TodoItemInput = Record<string, any> & TodoItemInput, TEvent extends { type: TodoItemEventName } = { type: TodoItemEventName }>(options: CreateTodoItemOptions<TContext, TEvent>): TodoItemMachine<TContext, TEvent>;
export function createTodoItem({ context = {}, parent }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  const states: Record<string, any> = {
    active: {
      remove(event: any) {
        enterRemoved(event);
      }
    },
    removed: {}
  };
  function enterRemoved(event: any) {
    machine.state = 'removed';
    machine.done = true;
    parent?.send({ type: 'removedTodo', data: event.data });
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'active',
    context,
    done: false,
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export type TodoListState = 'ready';
export type TodoListEventName = 'add' | 'draft' | 'removedTodo';
export type TodoListContextKey = 'todo' | 'draft';

export interface CreateTodoListOptions<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  context?: TContext;
  assigns: {
    draftTitle: (context: TContext, event: Extract<TEvent, { type: 'draft' }>) => unknown;
  };
}

export interface TodoListMachine<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  state: TodoListState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoListMachine<TContext, TEvent>) => void): () => void;
  start(): TodoListMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoList<TContext extends Record<TodoListContextKey, any> = Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName } = { type: TodoListEventName }>(options: CreateTodoListOptions<TContext, TEvent>): TodoListMachine<TContext, TEvent>;
export function createTodoList({ assigns, context = {} }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  const states: Record<string, any> = {
    ready: {
      add(event: any) {
        machine.context.todo?.stop();
        machine.context = {
          ...machine.context,
          todo: createTodoItem({
            context: {
              title: event.data,
              storage: machine.context.storage,
              id: machine
            },
            parent: machine
          }).start()
        };
        machine.state = 'ready';
      },
      draft(event: any) {
        machine.context.draft?.stop();
        machine.context = {
          ...machine.context,
          draft: createTodoItem({
            context: {
              title: assigns.draftTitle(machine.context, event),
              storage: machine.context.storage,
              id: makeId(machine.context, event)
            },
            parent: machine
          }).start()
        };
        machine.state = 'ready';
      },
      removedTodo() {
        machine.state = 'ready';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'ready',
    context,
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
      if(machine.context.todo) {
        machine.context.todo.stop();
      }
      if(machine.context.draft) {
        machine.context.draft.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
