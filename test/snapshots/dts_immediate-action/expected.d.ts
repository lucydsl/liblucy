import { Action, InvokeCreator, StateMachine } from 'xstate';

type MachineEventNames = 'first' | 'second';

export interface CreateMachineOptions<TContext, TEvent extends { type: MachineEventNames }> {
  actions: {
    log: Action<
      TContext,
      TEvent
    >
  }
}

export default function createMachine<TContext extends Record<any, any>, TEvent extends { type: MachineEventNames } = any>(options: CreateMachineOptions<TContext, TEvent>): StateMachine<TContext, any, TEvent>;
