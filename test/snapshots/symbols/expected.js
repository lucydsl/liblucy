export default function createMachine({ actions, assigns, context = {}, delays, guards, services } = {}) {
  const listeners = new Set();
  let loadingPromise;
  let loadedTimer;
  const states = {
    idle: {
      next(event) {
        if(guards.checkValid(machine.context, event) && guards.check(machine.context, event)) {
          actions.log(machine.context, event);
          machine.context = { ...machine.context, name: assigns.namer(machine.context, event) };
          enterLoading(event);
        }
      }
    },
    loading: {},
    loaded: {},
    homescreen: {}
  };
  function enterLoading(event) {
    machine.state = 'loading';
    actions.updateUI(machine.context, event);
    actions.log(machine.context, event);
    machine.context = { ...machine.context, count: assigns.incrementLoads(machine.context, event) };
    const promise = services.loadUsers(machine.context, event);
    loadingPromise = promise;
    promise.then(data => {
      if(loadingPromise !== promise) return;
      const event = { type: 'done', data };
      enterLoaded(event);
      notify();
    });
  }
  function enterLoaded(event) {
    machine.state = 'loaded';
    loadedTimer = setTimeout(() => {
      const event = { type: 'delay' };
      exitLoaded(event);
      actions.doLog(machine.context, event);
      enterHomescreen();
      notify();
    }, delays.wait(machine.context, event));
  }
  function exitLoaded(event) {
    actions.log(machine.context, event);
    clearTimeout(loadedTimer);
  }
  function enterHomescreen() {
    machine.state = 'homescreen';
    machine.context = { ...machine.context, todo: services.todoMachine().start() };
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'idle',
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
      notify();
      return machine;
    },
    stop() {
      loadingPromise = undefined;
      clearTimeout(loadedTimer);
      if(machine.context.todo) {
        machine.context.todo.stop();
      }
      listeners.clear();
    }
  };
  return machine;
}
