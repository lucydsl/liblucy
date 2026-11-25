export type CountdownState = 'ticking' | 'finished';
export interface CountdownInput {
  seconds: unknown;
  label: unknown;
}

export interface CreateCountdownOptions<TContext extends Record<string, any> & CountdownInput, TEvent extends { type: string }> {
  context: TContext;
  guards: {
    more: (context: TContext, event: TEvent) => boolean;
  };
  parent?: { send(event: { type: 'tick'; data?: unknown }): void };
}

export interface CountdownMachine<TContext extends Record<string, any> & CountdownInput, TEvent extends { type: string }> {
  state: CountdownState;
  context: TContext;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: CountdownMachine<TContext, TEvent>) => void): () => void;
  start(): CountdownMachine<TContext, TEvent>;
  stop(): void;
}

export function createCountdown<TContext extends Record<string, any> & CountdownInput = Record<string, any> & CountdownInput, TEvent extends { type: string } = { type: string }>(options: CreateCountdownOptions<TContext, TEvent>): CountdownMachine<TContext, TEvent>;

export type PlainState = 'only' | 'over';

export interface CreatePlainOptions<TContext extends Record<string, any>, TEvent extends { type: string }> {
  context?: TContext;
}

export interface PlainMachine<TContext extends Record<string, any>, TEvent extends { type: string }> {
  state: PlainState;
  context: TContext;
  done: boolean;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: PlainMachine<TContext, TEvent>) => void): () => void;
  start(): PlainMachine<TContext, TEvent>;
  stop(): void;
}

export function createPlain<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: string } = { type: string }>(options?: CreatePlainOptions<TContext, TEvent>): PlainMachine<TContext, TEvent>;

export type TimerState = 'idle' | 'running' | 'resting';
export type TimerEventName = 'start' | 'rest' | 'tick' | 'cancel';
export type TimerContextKey = 'remaining';

export interface CreateTimerOptions<TContext extends Record<TimerContextKey, any>, TEvent extends { type: TimerEventName }> {
  context?: TContext;
  guards: {
    keepGoing: (context: any, event: any) => boolean;
  };
}

export interface TimerMachine<TContext extends Record<TimerContextKey, any>, TEvent extends { type: TimerEventName }> {
  state: TimerState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TimerMachine<TContext, TEvent>) => void): () => void;
  start(): TimerMachine<TContext, TEvent>;
  stop(): void;
}

export function createTimer<TContext extends Record<TimerContextKey, any> = Record<TimerContextKey, any>, TEvent extends { type: TimerEventName } = { type: TimerEventName }>(options: CreateTimerOptions<TContext, TEvent>): TimerMachine<TContext, TEvent>;
