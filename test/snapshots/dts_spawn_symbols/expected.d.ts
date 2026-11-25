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
