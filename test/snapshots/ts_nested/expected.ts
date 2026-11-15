// Compiled from Lucy by lucyc.
// @ts-nocheck

export type PlayerState = 'stopped' | 'playing.running' | 'playing.paused';
export type PlayerEventName = 'play' | 'resume' | 'stop' | 'pause';

export interface CreatePlayerOptions<TContext extends Record<string, any>, TEvent extends { type: PlayerEventName }> {
  context?: TContext;
  actions: {
    load: (context: TContext, event: TEvent) => void;
    unload: (context: TContext, event: TEvent) => void;
    dim: (context: TContext, event: TEvent) => void;
  };
}

export interface PlayerMachine<TContext extends Record<string, any>, TEvent extends { type: PlayerEventName }> {
  state: PlayerState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: PlayerMachine<TContext, TEvent>) => void): () => void;
  start(): PlayerMachine<TContext, TEvent>;
  stop(): void;
}

export function createPlayer<TContext extends Record<string, any> = Record<string, any>, TEvent extends { type: PlayerEventName } = { type: PlayerEventName }>(options: CreatePlayerOptions<TContext, TEvent>): PlayerMachine<TContext, TEvent>;
export function createPlayer({ actions, context = {} }: any = {}): any {
  const listeners = new Set<(machine: any) => void>();
  const states: Record<string, any> = {
    stopped: {
      play(event: any) {
        enterPlaying(event);
      },
      resume(event: any) {
        enterPlaying(event, enterPlayingPaused);
      }
    },
    'playing.running': {
      pause(event: any) {
        enterPlayingPaused(event);
      },
      stop: playingOnStop
    },
    'playing.paused': {
      pause() {
        enterPlayingRunning();
      },
      stop: playingOnStop
    }
  };
  function enterPlaying(event: any, enterChild: (event: any) => void = enterPlayingRunning) {
    actions.load(machine.context, event);
    enterChild(event);
  }
  function exitPlaying(event: any) {
    actions.unload(machine.context, event);
  }
  function playingOnStop(event: any) {
    exitPlaying(event);
    machine.state = 'stopped';
  }
  function enterPlayingRunning() {
    machine.state = 'playing.running';
  }
  function enterPlayingPaused(event: any) {
    machine.state = 'playing.paused';
    actions.dim(machine.context, event);
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine: any = {
    state: 'stopped',
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
