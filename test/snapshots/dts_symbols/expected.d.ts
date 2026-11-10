export type MachineState = 'idle' | 'loading' | 'loaded' | 'homescreen';
export type MachineEventName = 'next';
export type MachineContextKey = 'name' | 'count' | 'todo';

export interface CreateMachineOptions<TContext extends Record<MachineContextKey, any>, TEvent extends { type: MachineEventName }> {
  context?: TContext;
  actions: {
    log: (context: TContext, event: TEvent) => void;
    updateUI: (context: TContext, event: TEvent) => void;
    doLog: (context: TContext, event: TEvent) => void;
  };
  assigns: {
    namer: (context: TContext, event: Extract<TEvent, { type: 'next' }>) => TContext['name'];
    incrementLoads: (context: TContext, event: TEvent) => TContext['count'];
  };
  delays: {
    wait: (context: TContext, event: TEvent) => number;
  };
  guards: {
    checkValid: (context: TContext, event: Extract<TEvent, { type: 'next' }>) => boolean;
    check: (context: TContext, event: Extract<TEvent, { type: 'next' }>) => boolean;
  };
  services: {
    loadUsers: (context: TContext, event: TEvent) => Promise<unknown>;
    todoMachine: () => { start(): unknown; stop(): void; send(event: unknown): void };
  };
}

export interface Machine<TContext extends Record<MachineContextKey, any>, TEvent extends { type: MachineEventName }> {
  state: MachineState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: Machine<TContext, TEvent>) => void): () => void;
  start(): Machine<TContext, TEvent>;
  stop(): void;
}

export default function createMachine<TContext extends Record<MachineContextKey, any> = Record<MachineContextKey, any>, TEvent extends { type: MachineEventName } = { type: MachineEventName }>(options: CreateMachineOptions<TContext, TEvent>): Machine<TContext, TEvent>;
