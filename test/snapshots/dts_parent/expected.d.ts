export type TodoItemState = 'active' | 'removed';
export type TodoItemEventName = 'remove' | 'rename';

export interface CreateTodoItemOptions<TContext extends Record<string, any>, TEvent extends { type: TodoItemEventName }> {
  context?: TContext;
  parent?: { send(event: { type: 'renamed' | 'removedTodo'; data?: unknown }): void };
}

export interface TodoItemMachine<TContext extends Record<string, any>, TEvent extends { type: TodoItemEventName }> {
  state: TodoItemState;
  context: TContext;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoItemMachine<TContext, TEvent>) => void): () => void;
  start(): TodoItemMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoItem<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: TodoItemEventName } = { type: TodoItemEventName }>(options?: CreateTodoItemOptions<TContext, TEvent>): TodoItemMachine<TContext, TEvent>;

export type TodoListState = 'ready' | 'empty';
export type TodoListEventName = 'add' | 'remove' | 'removedTodo';
export type TodoListContextKey = 'todo';

export interface CreateTodoListOptions<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  context?: TContext;
}

export interface TodoListMachine<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  state: TodoListState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoListMachine<TContext, TEvent>) => void): () => void;
  start(): TodoListMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoList<TContext extends Record<TodoListContextKey, any> = Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName } = { type: TodoListEventName }>(options?: CreateTodoListOptions<TContext, TEvent>): TodoListMachine<TContext, TEvent>;
