export type TodoItemState = 'active' | 'gone';
export type TodoItemEventName = 'rename' | 'touch' | 'report' | 'forward' | 'remove';
export interface TodoItemInput {
  id: unknown;
  list: unknown;
}

export interface CreateTodoItemOptions<TContext extends Record<string, any> & TodoItemInput, TEvent extends { type: TodoItemEventName }> {
  context: TContext;
  assigns: {
    summarise: (context: TContext, event: Extract<TEvent, { type: 'report' }>) => unknown;
  };
  parent?: { send(event: { type: 'touched' | 'report' | 'summary' | 'forwarded' | 'gone'; data?: unknown }): void };
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
