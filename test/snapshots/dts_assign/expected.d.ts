import { PartialAssigner, StateMachine } from 'xstate';

type MachineEventNames = 'go';

type MachineKnownContextKeys = 'prop';

export interface CreateMachineOptions<TContext extends Record<MachineKnownContextKeys, any>, TEvent extends { type: MachineEventNames }> {
  assigns: {
    val: PartialAssigner<
      TContext,
      TEvent extends Extract<TEvent, { type: 'go' }> ? Extract<TEvent, { type: 'go' }> : TEvent,
      'prop'
    >
  }
}

export default function createMachine<TContext extends Record<MachineKnownContextKeys, any>, TEvent extends { type: MachineEventNames } = any>(options: CreateMachineOptions<TContext, TEvent>): StateMachine<TContext, any, TEvent>;
