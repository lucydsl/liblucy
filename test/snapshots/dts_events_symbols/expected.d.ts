import type { User } from './types';

export type SessionState = 'anonymous' | 'active' | 'leaving';
export interface SessionEvents {
  login: { type: 'login'; data: User };
  rename: { type: 'rename'; data: string };
  resize: { type: 'resize'; data: number };
  logout: { type: 'logout' };
  refresh: { type: 'refresh' };
}
export type SessionEventName = keyof SessionEvents;
export type SessionEvent = SessionEvents[SessionEventName];
export type SessionAnyEvent = SessionEvent | { type: 'start' } | { type: 'delay' } | { type: 'done'; data?: unknown } | { type: 'error'; data: unknown };
export type SessionContextKey = 'user' | 'last' | 'name' | 'size' | 'saved';
export interface SessionEventData {
  user: User;
  last: User | string | number;
  size: number;
}

export interface CreateSessionOptions<TContext extends Record<SessionContextKey, any>> {
  context?: { [K in keyof TContext]: K extends keyof SessionEventData ? (SessionEventData[K] extends TContext[K] ? TContext[K] : TContext[K] extends SessionEventData[K] ? never : SessionEventData[K]) : TContext[K] };
  actions: {
    track: (context: TContext, event: SessionAnyEvent) => void;
    goodbye: (context: TContext, event: SessionEvents['logout' | 'refresh']) => void;
    report: (context: TContext, event: SessionAnyEvent) => void;
  };
  assigns: {
    trimmed: (context: TContext, event: SessionEvents['rename']) => TContext['name'];
  };
  delays: {
    idleTime: (context: TContext, event: SessionAnyEvent) => number;
  };
  guards: {
    allowed: (context: TContext, event: SessionEvents['login' | 'rename']) => boolean;
  };
  services: {
    save: (context: TContext, event: SessionAnyEvent) => Promise<unknown>;
  };
}

export interface SessionMachine<TContext extends Record<SessionContextKey, any>> {
  state: SessionState;
  context: TContext;
  send(event: SessionEvent | 'logout' | 'refresh'): void;
  subscribe(listener: (machine: SessionMachine<TContext>) => void): () => void;
  start(): SessionMachine<TContext>;
  stop(): void;
}

export function createSession<TContext extends Record<SessionContextKey, any> = Record<SessionContextKey, any>>(options: CreateSessionOptions<TContext>): SessionMachine<TContext>;

export type PlainState = 'off' | 'on';
export type PlainEventName = 'toggle';

export interface CreatePlainOptions<TContext extends Record<string, any>, TEvent extends { type: PlainEventName }> {
  context?: TContext;
  guards: {
    ready: (context: TContext, event: Extract<TEvent, { type: 'toggle' }>) => boolean;
  };
}

export interface PlainMachine<TContext extends Record<string, any>, TEvent extends { type: PlainEventName }> {
  state: PlainState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: PlainMachine<TContext, TEvent>) => void): () => void;
  start(): PlainMachine<TContext, TEvent>;
  stop(): void;
}

export function createPlain<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: PlainEventName } = { type: PlainEventName }>(options: CreatePlainOptions<TContext, TEvent>): PlainMachine<TContext, TEvent>;
