// Compiled from Lucy by lucyc.
// @ts-nocheck

import { getUser, setUser } from './user.js';

export type MachineState = 'loading' | 'ready' | 'error';
export type MachineEventName = 'again';
export type MachineContextKey = 'user';

export interface CreateMachineOptions<TContext extends Record<MachineContextKey, any>, TEvent extends { type: MachineEventName }> {
  context?: TContext;
}

export interface Machine<TContext extends Record<MachineContextKey, any>, TEvent extends { type: MachineEventName }> {
  state: MachineState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: Machine<TContext, TEvent>) => void): () => void;
  start(): Machine<TContext, TEvent>;
  stop(): void;
}

export default function createMachine<TContext extends Record<MachineContextKey, any> = Record<MachineContextKey, any>, TEvent extends { type: MachineEventName } = { type: MachineEventName }>(options?: CreateMachineOptions<TContext, TEvent>): Machine<TContext, TEvent>;
export default function createMachine({ context = {} }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  let loadingPromise: any;
  const states: Record<string, any> = {
    loading: {
      again(event: any) {
        exitLoading();
        enterLoading(event);
      }
    },
    ready: {},
    error: {}
  };
  function enterLoading(event: any) {
    machine.state = 'loading';
    const promise = getUser(machine.context, event);
    loadingPromise = promise;
    promise.then((data: any) => {
      if(loadingPromise !== promise) return;
      const event = { type: 'done', data };
      run(() => {
        exitLoading();
        machine.context = { ...machine.context, user: setUser(machine.context, event) };
        machine.state = 'ready';
      });
    }, () => {
      if(loadingPromise !== promise) return;
      run(() => {
        exitLoading();
        machine.state = 'error';
      });
    });
  }
  function exitLoading() {
    loadingPromise = undefined;
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  function run(step: () => void) {
    busy = true;
    try {
      step();
      notify();
      busy = false;
      if(queue.length) {
        machine.send(queue.shift());
      }
    } finally {
      queue.length = 0;
      busy = false;
    }
  }
  const machine: any = {
    state: 'loading',
    context,
    send(event: any) {
      if(typeof event === 'string') {
        event = { type: event };
      }
      if(busy) {
        queue.push(event);
        return;
      }
      busy = true;
      try {
        do {
          const handler = states[machine.state][event.type];
          if(handler) {
            handler(event);
            notify();
          }
        } while((event = queue.shift()));
      } finally {
        queue.length = 0;
        busy = false;
      }
    },
    subscribe(listener: any) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      run(() => enterLoading({ type: 'start' }));
      return machine;
    },
    stop() {
      loadingPromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
