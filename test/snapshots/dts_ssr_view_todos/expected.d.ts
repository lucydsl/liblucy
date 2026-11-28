export type TodoItemState = 'active' | 'removed';
export type TodoItemEventName = 'toggle' | 'remove';
export type TodoItemContextKey = 'done';
export interface TodoItemInput {
  title: unknown;
  list: unknown;
}

export interface CreateTodoItemOptions<TContext extends Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  context: TContext;
}

export interface TodoItemMachine<TContext extends Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  state: TodoItemState;
  context: TContext;
  render(): string;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoItemMachine<TContext, TEvent>) => void): () => void;
  start(): TodoItemMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoItem<TContext extends Record<TodoItemContextKey, any> & TodoItemInput = Record<TodoItemContextKey, any> & TodoItemInput, TEvent extends { type: TodoItemEventName } = { type: TodoItemEventName }>(options: CreateTodoItemOptions<TContext, TEvent>): TodoItemMachine<TContext, TEvent>;

export type TodoListState = 'ready';
export type TodoListEventName = 'add' | 'removedTodo';
export type TodoListContextKey = 'todos';

export interface CreateTodoListOptions<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  context?: TContext;
}

export interface TodoListMachine<TContext extends Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName }> {
  state: TodoListState;
  context: TContext;
  render(): string;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TodoListMachine<TContext, TEvent>) => void): () => void;
  start(): TodoListMachine<TContext, TEvent>;
  stop(): void;
}

export function createTodoList<TContext extends Record<TodoListContextKey, any> = Record<TodoListContextKey, any>, TEvent extends { type: TodoListEventName } = { type: TodoListEventName }>(options?: CreateTodoListOptions<TContext, TEvent>): TodoListMachine<TContext, TEvent>;
