import { Action, StateMachine } from 'xstate';

export interface CreateMachineOptions<TContext, TEvent> {
  actions: {
    log: Action<
      TContext,
      TEvent
    >
  }
}

export default function createMachine<TContext extends Record<any, any>, TEvent>(options: CreateMachineOptions<TContext, TEvent>): StateMachine<TContext, any, TEvent>;
