// Compiled from Lucy by lucyc.
// @ts-nocheck

export type MachineState = 'enabled' | 'disabled';
export type MachineEventName = 'toggle';

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
  const states: Record<string, any> = {
    enabled: {
      toggle() {
        machine.state = 'disabled';
      }
    },
    disabled: {
      toggle() {
        machine.state = 'enabled';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'disabled',
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
