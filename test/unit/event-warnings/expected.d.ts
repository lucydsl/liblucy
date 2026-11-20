Event [pong] in state [only] is not declared by machine [kid].
Event [unused] is declared by machine [kid], but none of its states handles it.
send(child, nope) in state [idle] sends an event that machine [kid] does not declare.
assign(last) in state [idle] stores the data of event [stop], which is declared without any.
send(kid, ping, ...) in state [idle] gives data to an event that machine [quiet] declares without any.
export type KidState = 'only';
export interface KidEvents {
  ping: { type: 'ping'; data: number };
  unused: { type: 'unused' };
  pong: { type: 'pong'; data?: unknown };
}
export type KidEventName = keyof KidEvents;
export type KidEvent = KidEvents[KidEventName];

export interface CreateKidOptions<TContext extends Record<string, any>> {
  context?: TContext;
}

export interface KidMachine<TContext extends Record<string, any>> {
  state: KidState;
  context: TContext;
  send(event: KidEvent | 'unused' | 'pong'): void;
  subscribe(listener: (machine: KidMachine<TContext>) => void): () => void;
  start(): KidMachine<TContext>;
  stop(): void;
}

export function createKid<TContext extends Record<string, any> = Record<string, any>>(options?: CreateKidOptions<TContext>): KidMachine<TContext>;

export type ParentState = 'idle';
export interface ParentEvents {
  go: { type: 'go'; data: number };
  stop: { type: 'stop' };
}
export type ParentEventName = keyof ParentEvents;
export type ParentEvent = ParentEvents[ParentEventName];
export type ParentContextKey = 'child' | 'last';

export interface CreateParentOptions<TContext extends Record<ParentContextKey, any>> {
  context?: TContext;
}

export interface ParentMachine<TContext extends Record<ParentContextKey, any>> {
  state: ParentState;
  context: TContext;
  send(event: ParentEvent | 'stop'): void;
  subscribe(listener: (machine: ParentMachine<TContext>) => void): () => void;
  start(): ParentMachine<TContext>;
  stop(): void;
}

export function createParent<TContext extends Record<ParentContextKey, any> = Record<ParentContextKey, any>>(options?: CreateParentOptions<TContext>): ParentMachine<TContext>;

export type QuietState = 'only';
export interface QuietEvents {
  ping: { type: 'ping' };
  set: { type: 'set'; data: number };
}
export type QuietEventName = keyof QuietEvents;
export type QuietEvent = QuietEvents[QuietEventName];

export interface CreateQuietOptions<TContext extends Record<string, any>> {
  context?: TContext;
}

export interface QuietMachine<TContext extends Record<string, any>> {
  state: QuietState;
  context: TContext;
  send(event: QuietEvent | 'ping'): void;
  subscribe(listener: (machine: QuietMachine<TContext>) => void): () => void;
  start(): QuietMachine<TContext>;
  stop(): void;
}

export function createQuiet<TContext extends Record<string, any> = Record<string, any>>(options?: CreateQuietOptions<TContext>): QuietMachine<TContext>;

export type TellerState = 'idle';
export type TellerEventName = 'one' | 'two' | 'three';
export type TellerContextKey = 'kid';

export interface CreateTellerOptions<TContext extends Record<TellerContextKey, any>, TEvent extends { type: TellerEventName }> {
  context?: TContext;
}

export interface TellerMachine<TContext extends Record<TellerContextKey, any>, TEvent extends { type: TellerEventName }> {
  state: TellerState;
  context: TContext;
  send(event: TEvent | TEvent['type']): void;
  subscribe(listener: (machine: TellerMachine<TContext, TEvent>) => void): () => void;
  start(): TellerMachine<TContext, TEvent>;
  stop(): void;
}

export function createTeller<TContext extends Record<TellerContextKey, any> = Record<TellerContextKey, any>, TEvent extends { type: TellerEventName } = { type: TellerEventName }>(options?: CreateTellerOptions<TContext, TEvent>): TellerMachine<TContext, TEvent>;
