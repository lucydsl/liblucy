export type TodoItemState = 'active' | 'removed';
export type TodoItemEventName = 'remove';
export interface TodoItemInput {
  title: unknown;
  list: unknown;
}

export interface CreateTodoItemOptions<TContext extends Record<string, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  context: TContext;
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

export type PlainState = 'only';

export interface CreatePlainOptions<TContext extends Record<string, any>, TEvent extends { type: string }> {
  context?: TContext;
}

export interface PlainMachine<TContext extends Record<string, any>, TEvent extends { type: string }> {
  state: PlainState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: PlainMachine<TContext, TEvent>) => void): () => void;
  start(): PlainMachine<TContext, TEvent>;
  stop(): void;
}

export function createPlain<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: string } = { type: string }>(options?: CreatePlainOptions<TContext, TEvent>): PlainMachine<TContext, TEvent>;

export type TodoListState = 'ready';
export type TodoListEventName = 'add' | 'removedTodo' | 'drop';
export type TodoListContextKey = 'others' | 'todos';

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
