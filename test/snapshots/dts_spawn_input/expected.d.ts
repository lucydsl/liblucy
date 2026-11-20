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
