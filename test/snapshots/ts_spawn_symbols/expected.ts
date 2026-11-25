// Compiled from Lucy by lucyc.
// @ts-nocheck

import { log, loadSub } from './util.js';

export type SubtaskState = 'open' | 'closed';
export type SubtaskEventName = 'close';

export interface CreateSubtaskOptions<TContext extends Record<string, any>, TEvent extends { type: SubtaskEventName }> {
  context?: TContext;
  guards: {
    canClose: (context: TContext, event: Extract<TEvent, { type: 'close' }>) => boolean;
  };
}

export interface SubtaskMachine<TContext extends Record<string, any>, TEvent extends { type: SubtaskEventName }> {
  state: SubtaskState;
  context: TContext;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: SubtaskMachine<TContext, TEvent>) => void): () => void;
  start(): SubtaskMachine<TContext, TEvent>;
  stop(): void;
}

export function createSubtask<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: SubtaskEventName } = { type: SubtaskEventName }>(options: CreateSubtaskOptions<TContext, TEvent>): SubtaskMachine<TContext, TEvent>;
export function createSubtask({ context = {}, guards }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  const states: Record<string, any> = {
    open: {
      close(event: any) {
        if(guards.canClose(machine.context, event)) {
          machine.state = 'closed';
          machine.done = true;
        }
      }
    },
    closed: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'open',
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

export type TodoItemState = 'active' | 'removed';
export type TodoItemEventName = 'remove' | 'split';
export type TodoItemContextKey = 'reason' | 'subtasks';
export interface TodoItemInput {
  title: unknown;
}

export interface CreateTodoItemOptions<TContext extends Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  context: TContext;
  actions: {
    log: (context: TContext, event: Extract<TEvent, { type: 'remove' }>) => void;
  };
  assigns: {
    why: (context: TContext, event: Extract<TEvent, { type: 'remove' }>) => TContext['reason'];
  };
  delays: {
    wait: (context: TContext, event: TEvent) => number;
  };
  guards: {
    canDelete: (context: TContext, event: Extract<TEvent, { type: 'remove' }>) => boolean;
    canCloseSub: (context: any, event: any) => boolean;
  };
  services: {
    save: (context: TContext, event: TEvent) => Promise<unknown>;
  };
}

export interface TodoItemMachine<TContext extends Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  state: TodoItemState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoItemMachine<TContext, TEvent>) => void): () => void;
  start(): TodoItemMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoItem<TContext extends Record<TodoItemContextKey, any> & TodoItemInput = Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName } = { type: TodoItemEventName }>(options: CreateTodoItemOptions<TContext, TEvent>): TodoItemMachine<TContext, TEvent>;
export function createTodoItem({ actions, assigns, context = {}, delays, guards, services }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  let activeTimer: ReturnType<typeof setTimeout> | undefined;
  let removedPromise: any;
  const states: Record<string, any> = {
    active: {
      remove(event: any) {
        if(guards.canDelete(machine.context, event)) {
          exitActive();
          actions.log(machine.context, event);
          machine.context = { ...machine.context, reason: assigns.why(machine.context, event) };
          enterRemoved(event);
        }
      },
      split(event: any) {
        exitActive();
        machine.context = {
          ...machine.context,
          subtasks: [
            ...(machine.context.subtasks ?? []),
            createSubtask({
              guards: {
                canClose: guards.canCloseSub
              }
            }).start()
          ]
        };
        enterActive(event);
      }
    },
    removed: {}
  };
  function enterActive(event: any) {
    machine.state = 'active';
    activeTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        exitActive();
        enterActive(event);
      });
    }, delays.wait(machine.context, event));
  }
  function exitActive() {
    clearTimeout(activeTimer);
  }
  function enterRemoved(event: any) {
    machine.state = 'removed';
    const promise = services.save(machine.context, event);
    removedPromise = promise;
    promise.then((data: any) => {
      if(removedPromise !== promise) return;
      const event = { type: 'done', data };
      run(() => {
        enterActive(event);
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
    state: 'active',
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
      run(() => enterActive({ type: 'start' }));
      return machine;
    },
    stop() {
      clearTimeout(activeTimer);
      removedPromise = undefined;
      for(const actor of machine.context.subtasks ?? []) {
        actor.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export type TodoListState = 'ready';
export type TodoListEventName = 'add' | 'only';
export type TodoListContextKey = 'todos' | 'one';

export interface CreateTodoListOptions<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  context?: TContext;
  assigns: {
    reasonFor: (context: any, event: any) => unknown;
  };
  delays: {
    todoWait: (context: any, event: any) => number;
  };
  guards: {
    canAdd: (context: TContext, event: Extract<TEvent, { type: 'add' }>) => boolean;
    canDeleteTodo: (context: any, event: any) => boolean;
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
export function createTodoList({ assigns, context = {}, delays, guards }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  const states: Record<string, any> = {
    ready: {
      add(event: any) {
        if(guards.canAdd(machine.context, event)) {
          machine.context = {
            ...machine.context,
            todos: [
              ...(machine.context.todos ?? []),
              createTodoItem({
                context: {
                  title: event.data
                },
                actions: {
                  log
                },
                assigns: {
                  why: assigns.reasonFor
                },
                delays: {
                  wait: delays.todoWait
                },
                guards: {
                  canDelete: guards.canDeleteTodo,
                  canCloseSub: guards.canAdd
                },
                services: {
                  save: loadSub
                }
              }).start()
            ]
          };
          machine.state = 'ready';
        }
      },
      only() {
        machine.context.one?.stop();
        machine.context = {
          ...machine.context,
          one: createSubtask({
            guards: {
              canClose: guards.canAdd
            }
          }).start()
        };
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
      for(const actor of machine.context.todos ?? []) {
        actor.stop();
      }
      if(machine.context.one) {
        machine.context.one.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
