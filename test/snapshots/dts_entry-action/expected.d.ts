export type MachineState = 'error';

export interface CreateMachineOptions<TContext extends Record<string, any>, TEvent extends { type: string }> {
  context?: TContext;
  actions: {
    log: (context: TContext, event: TEvent) => void;
  };
}

export interface Machine<TContext extends Record<string, any>, TEvent extends { type: string }> {
  state: MachineState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: Machine<TContext, TEvent>) => void): () => void;
  start(): Machine<TContext, TEvent>;
  stop(): void;
}

export default function createMachine<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: string } = { type: string }>(options: CreateMachineOptions<TContext, TEvent>): Machine<TContext, TEvent>;
