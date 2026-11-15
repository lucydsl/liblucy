import { getUser, setUser } from './user.js';

export default function createMachine({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
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
      run(() => {
        exitLoading();
        machine.context = { ...machine.context, user: setUser(machine.context, event) };
        machine.state = 'ready';
      });
    }, () => {
      if(loadingPromise !== promise) return;
      run(() => {
        exitLoading();
        machine.state = 'error';
      });
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
  function run(step) {
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
  const machine = {
    state: 'loading',
    context,
    send(event) {
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
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    start() {
      run(() => enterLoading({ type: 'start' }));
      return machine;
    },
    stop() {
      loadingPromise = undefined;
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
