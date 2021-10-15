import { Action, DelayConfig, ConditionPredicate, InvokeCreator, StateMachine } from 'xstate';

type MachineEventNames = 'idle' | 'loading' | 'loaded' | 'homescreen';

type MachineKnownContextKeys = 'name' | 'count' | 'todo';

export interface CreateMachineOptions<TContext, TEvent extends { type: MachineEventNames }> {
  actions: {
    log: Action<
      TContext,
      TEvent extends Extract<TEvent, { type: 'next' }> ? Extract<TEvent, { type: 'next' }> : TEvent
    >,
    updateUI: Action<
      TContext,
      TEvent extends Extract<TEvent, { type: 'next' }> ? Extract<TEvent, { type: 'next' }> : TEvent
    >
  },
  delays: {
    wait: DelayConfig<TContext, TEvent>
  },
  guards: {
    check: ConditionPredicate<
      TContext,
      TEvent extends Extract<TEvent, { type: 'next' }> ? Extract<TEvent, { type: 'next' }> : TEvent
    >
  },
  services: {
    loadUsers: InvokeCreator<TContext, TEvent>,
    todoMachine: StateMachine<TContext, any, TEvent>
  }
}

export default function createMachine<TContext extends Record<MachineKnownContextKeys, any>, TEvent extends { type: MachineEventNames } = any>(options: CreateMachineOptions<TContext, TEvent>): StateMachine<TContext, any, TEvent>;
