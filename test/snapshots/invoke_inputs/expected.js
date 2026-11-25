import { describe } from './util.js';

export function createCountdown({ context = {}, guards, parent } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let tickingTimer;
  let tickingTimer2;
  const states = {
    ticking: {},
    finished: {}
  };
  function enterTicking() {
    machine.state = 'ticking';
    tickingTimer = setTimeout(() => {
      const event = { type: 'delay' };
      if(guards.more(machine.context, event)) {
        run(() => {
          exitTicking();
          parent?.send({ type: 'tick', data: machine.context.seconds });
          enterTicking();
        });
      }
    }, 1000);
    tickingTimer2 = setTimeout(() => {
      run(() => {
        exitTicking();
        machine.state = 'finished';
        machine.done = true;
      });
    }, 2000);
  }
  function exitTicking() {
    clearTimeout(tickingTimer);
    clearTimeout(tickingTimer2);
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
    state: 'ticking',
    context,
    done: false,
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
      run(enterTicking);
      return machine;
    },
    stop() {
      clearTimeout(tickingTimer);
      clearTimeout(tickingTimer2);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createPlain({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {},
    over: {}
  };
  function enterOnly() {
    machine.state = 'only';
    machine.state = 'over';
    machine.done = true;
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'only',
    context,
    done: false,
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
      enterOnly();
      notify();
      return machine;
    },
    stop() {
      listeners.clear();
    }
  };
  return machine;
}

export function createTimer({ context = {}, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let runningChild;
  let restingChild;
  const states = {
    idle: {
      start(event) {
        enterRunning(event);
      },
      rest() {
        enterResting();
      }
    },
    running: {
      tick(event) {
        machine.context = { ...machine.context, remaining: event.data };
      },
      cancel() {
        exitRunning();
        machine.state = 'idle';
      }
    },
    resting: {}
  };
  function enterRunning(event) {
    machine.state = 'running';
    const child = createCountdown({
      context: {
        seconds: event.data,
        label: describe(machine.context, event)
      },
      guards: {
        more: guards.keepGoing
      },
      parent: machine
    });
    runningChild = child;
    child.subscribe(() => {
      if(child.done) {
        run(() => {
          exitRunning();
          machine.state = 'idle';
        });
      }
    });
    child.start();
  }
  function exitRunning() {
    runningChild.stop();
  }
  function enterResting() {
    machine.state = 'resting';
    const child = createPlain();
    restingChild = child;
    child.subscribe(() => {
      if(child.done) {
        run(() => {
          exitResting();
          machine.state = 'idle';
        });
      }
    });
    child.start();
  }
  function exitResting() {
    restingChild.stop();
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  function run(step) {
    if(busy) {
      step();
      return;
    }
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
    state: 'idle',
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
      notify();
      return machine;
    },
    stop() {
      if(runningChild) {
        runningChild.stop();
      }
      if(restingChild) {
        restingChild.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
