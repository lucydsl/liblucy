// Compiled from Lucy by lucyc.
// @ts-nocheck

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
export function createSession({ actions, assigns, context = {}, delays, guards, services }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let activeTimer: ReturnType<typeof setTimeout> | undefined;
  let leavingPromise: any;
  const states: Record<string, any> = {
    anonymous: {
      login(event: any) {
        if(guards.allowed(machine.context, event)) {
          machine.context = { ...machine.context, user: event.data };
          machine.context = { ...machine.context, last: event.data };
          enterActive(event);
        }
      }
    },
    active: {
      rename(event: any) {
        if(guards.allowed(machine.context, event)) {
          machine.context = { ...machine.context, name: assigns.trimmed(machine.context, event) };
          machine.context = { ...machine.context, last: event.data };
        }
      },
      resize(event: any) {
        machine.context = { ...machine.context, size: event.data };
        machine.context = { ...machine.context, last: event.data };
      },
      logout(event: any) {
        exitActive();
        actions.goodbye(machine.context, event);
        enterLeaving(event);
      },
      refresh(event: any) {
        exitActive();
        actions.goodbye(machine.context, event);
        enterActive(event);
      }
    },
    leaving: {}
  };
  function enterActive(event: any) {
    machine.state = 'active';
    actions.track(machine.context, event);
    activeTimer = setTimeout(() => {
      const event = { type: 'delay' };
      exitActive();
      enterLeaving(event);
      notify();
    }, delays.idleTime(machine.context, event));
  }
  function exitActive() {
    clearTimeout(activeTimer);
  }
  function enterLeaving(event: any) {
    machine.state = 'leaving';
    const promise = services.save(machine.context, event);
    leavingPromise = promise;
    promise.then((data: any) => {
      if(leavingPromise !== promise) return;
      const event = { type: 'done', data };
      machine.context = { ...machine.context, saved: event.data };
      machine.state = 'anonymous';
      notify();
    }, (error: any) => {
      if(leavingPromise !== promise) return;
      const event = { type: 'error', data: error };
      actions.report(machine.context, event);
      enterActive(event);
      notify();
    });
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'anonymous',
    context,
    send(event: any) {
      if(typeof event === 'string') {
        event = { type: event };
      }
      const handler = states[machine.state][event.type];
      if(handler) {
        handler(event);
        notify();
      }
    },
    subscribe(listener: any) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      notify();
      return machine;
    },
    stop() {
      clearTimeout(activeTimer);
      leavingPromise = undefined;
      listeners.clear();
    }
  };
  return machine;
}

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
export function createPlain({ context = {}, guards }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  const states: Record<string, any> = {
    off: {
      toggle(event: any) {
        if(guards.ready(machine.context, event)) {
          machine.state = 'on';
        }
      }
    },
    on: {
      toggle() {
        machine.state = 'off';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'off',
    context,
    send(event: any) {
      if(typeof event === 'string') {
        event = { type: event };
      }
      const handler = states[machine.state][event.type];
      if(handler) {
        handler(event);
        notify();
      }
    },
    subscribe(listener: any) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}
