import { getUser, setUser } from './user.js';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let loadingPromise;
  const states = {
    loading: {
      again(event) {
        exitLoading();
        enterLoading(event);
      }
    },
    ready: {},
    error: {}
  };
  function enterLoading(event) {
    machine.state = 'loading';
    const promise = getUser(machine.context, event);
    loadingPromise = promise;
    promise.then(data => {
      if(loadingPromise !== promise) return;
      const event = { type: 'done', data };
      exitLoading();
      machine.context = { ...machine.context, user: setUser(machine.context, event) };
      machine.state = 'ready';
      notify();
    }, () => {
      if(loadingPromise !== promise) return;
      exitLoading();
      machine.state = 'error';
      notify();
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
  const machine = {
    state: 'loading',
    context,
    send(event) {
      if(typeof event === 'string') {
        event = { type: event };
      }
      const handler = states[machine.state][event.type];
      if(handler) {
        handler(event);
        notify();
      }
    },
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      enterLoading({ type: 'start' });
      notify();
      return machine;
    },
    stop() {
      loadingPromise = undefined;
      listeners.clear();
    }
  };
  return machine;
}
