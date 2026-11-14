import type { User } from './types';
import type { Amount } from './util';

export type CounterState = 'idle' | 'active';
export interface CounterEvents {
  set: { type: 'set'; data: Amount };
  login: { type: 'login'; data: User };
  reset: { type: 'reset' };
}
export type CounterEventName = keyof CounterEvents;
export type CounterEvent = CounterEvents[CounterEventName];
export type CounterContextKey = 'count' | 'user';
export interface CounterEventData {
  count: Amount;
  user: User;
}

export interface CreateCounterOptions<TContext extends Record<CounterContextKey, any>> {
  context?: { [K in keyof TContext]: K extends keyof CounterEventData ? (CounterEventData[K] extends TContext[K] ? TContext[K] : TContext[K] extends CounterEventData[K] ? never : CounterEventData[K]) : TContext[K] };
}

export interface CounterMachine<TContext extends Record<CounterContextKey, any>> {
  state: CounterState;
  context: TContext;
  send(event: CounterEvent | 'reset'): void;
  subscribe(listener: (machine: CounterMachine<TContext>) => void): () => void;
  start(): CounterMachine<TContext>;
  stop(): void;
}

export function createCounter<TContext extends Record<CounterContextKey, any> = Record<CounterContextKey, any>>(options?: CreateCounterOptions<TContext>): CounterMachine<TContext>;
