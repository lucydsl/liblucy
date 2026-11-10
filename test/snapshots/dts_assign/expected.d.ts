export type MachineState = 'first' | 'second';
export type MachineEventName = 'go';
export type MachineContextKey = 'prop';

export interface CreateMachineOptions<TContext extends Record<MachineContextKey, any>, TEvent extends { type: MachineEventName }> {
  context?: TContext;
  assigns: {
    val: (context: TContext, event: Extract<TEvent, { type: 'go' }>) => TContext['prop'];
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
