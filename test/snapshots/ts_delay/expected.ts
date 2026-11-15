// Compiled from Lucy by lucyc.
// @ts-nocheck

import { calcLightDelay } from './util';

export type MachineState = 'green' | 'yellow' | 'red';
export type MachineEventName = 'go';

export interface CreateMachineOptions<TContext extends Record<string, any>, TEvent extends { type: MachineEventName }> {
  context?: TContext;
}

export interface Machine<TContext extends Record<string, any>, TEvent extends { type: MachineEventName }> {
  state: MachineState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: Machine<TContext, TEvent>) => void): () => void;
  start(): Machine<TContext, TEvent>;
  stop(): void;
}

export default function createMachine<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: MachineEventName } = { type: MachineEventName }>(options?: CreateMachineOptions<TContext, TEvent>): Machine<TContext, TEvent>;
export default function createMachine({ context = {} }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  let busy = false;
  const queue: any[] = [];
  let greenTimer: ReturnType<typeof setTimeout> | undefined;
  let yellowTimer: ReturnType<typeof setTimeout> | undefined;
  let redTimer: ReturnType<typeof setTimeout> | undefined;
  const states: Record<string, any> = {
    green: {},
    yellow: {
      go() {
        exitYellow();
        enterGreen();
      }
    },
    red: {}
  };
  function enterGreen() {
    machine.state = 'green';
    greenTimer = setTimeout(() => {
      run(() => {
        enterYellow();
      });
    }, 1000);
  }
  function enterYellow() {
    machine.state = 'yellow';
    yellowTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        exitYellow();
        enterRed(event);
      });
    }, 500);
  }
  function exitYellow() {
    clearTimeout(yellowTimer);
  }
  function enterRed(event: any) {
    machine.state = 'red';
    redTimer = setTimeout(() => {
      run(() => {
        enterGreen();
      });
    }, calcLightDelay(machine.context, event));
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
    state: 'green',
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
      run(enterGreen);
      return machine;
    },
    stop() {
      clearTimeout(greenTimer);
      clearTimeout(yellowTimer);
      clearTimeout(redTimer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
