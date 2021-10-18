import { PartialAssigner, StateMachine } from 'xstate';

type MachineKnownContextKeys = 'prop';

export interface CreateMachineOptions<TContext extends Record<MachineKnownContextKeys, any>, TEvent> {
  assigns: {
    val: PartialAssigner<
      TContext,
      TEvent,
      'prop'
    >
  }
}

export default function createMachine<TContext extends Record<MachineKnownContextKeys, any>, TEvent>(options: CreateMachineOptions<TContext, TEvent>): StateMachine<TContext, any, TEvent>;
