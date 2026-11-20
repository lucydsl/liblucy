Machine [parent] spawns machine [kid], which needs options that nothing supplies to it (actions.log, delays.wait, guards.ok), so it will throw when it uses them.
Machine [parent] invokes machine [timer], which needs options that nothing supplies to it (actions.ring), so it will throw when it uses them.
Machine [reporter] sends its parent the event [unknown], which machine [boss], which spawns it, does not declare.
spawn(card) for [b] in state [idle] doesn't give machine [card] its input [owner].
spawn(card) for [c] in state [idle] doesn't give machine [card] its input [title].
spawn(card) for [c] in state [idle] doesn't give machine [card] its input [owner].
spawn(card) for [d] in state [idle] gives [colour], which machine [card] doesn't declare as an input.
invoke(card) in state [waiting] doesn't give machine [card] its input [title]: an invoke can't give any.
invoke(card) in state [waiting] doesn't give machine [card] its input [owner]: an invoke can't give any.
export function createKid({ actions, context = {}, delays, guards } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let waitingTimer;
  const states = {
    waiting: {
      go(event) {
        if(guards.ok(machine.context, event)) {
          exitWaiting();
          actions.log(machine.context, event);
          enterWaiting(event);
        }
      }
    }
  };
  function enterWaiting(event) {
    machine.state = 'waiting';
    waitingTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        exitWaiting();
        enterWaiting(event);
      });
    }, delays.wait(machine.context, event));
  }
  function exitWaiting() {
    clearTimeout(waitingTimer);
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
    state: 'waiting',
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
      run(() => enterWaiting({ type: 'start' }));
      return machine;
    },
    stop() {
      clearTimeout(waitingTimer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createTimer({ actions, context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let runningTimer;
  const states = {
    running: {},
    over: {}
  };
  function enterRunning() {
    machine.state = 'running';
    runningTimer = setTimeout(() => {
      const event = { type: 'delay' };
      run(() => {
        actions.ring(machine.context, event);
        machine.state = 'over';
        machine.done = true;
      });
    }, 1000);
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
    state: 'running',
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
      run(enterRunning);
      return machine;
    },
    stop() {
      clearTimeout(runningTimer);
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createPlain({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {
      ping() {
        machine.state = 'only';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'only',
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
      listeners.clear();
    }
  };
  return machine;
}

export function createParent({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let waitingChild;
  const states = {
    idle: {
      again() {
        machine.context.child?.stop();
        machine.context = { ...machine.context, child: createKid().start() };
        enterIdle();
      },
      wait() {
        enterWaiting();
      }
    },
    waiting: {}
  };
  function enterIdle() {
    machine.state = 'idle';
    machine.context.child?.stop();
    machine.context = { ...machine.context, child: createKid().start() };
    machine.context.other?.stop();
    machine.context = { ...machine.context, other: createPlain().start() };
  }
  function enterWaiting() {
    machine.state = 'waiting';
    const child = createTimer();
    waitingChild = child;
    child.subscribe(() => {
      if(child.done) {
        run(() => {
          exitWaiting();
          enterIdle();
        });
      }
    });
    child.start();
  }
  function exitWaiting() {
    waitingChild.stop();
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
      run(enterIdle);
      return machine;
    },
    stop() {
      if(waitingChild) {
        waitingChild.stop();
      }
      if(machine.context.child) {
        machine.context.child.stop();
      }
      if(machine.context.other) {
        machine.context.other.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createReporter({ context = {}, parent } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    only: {
      go(event) {
        parent?.send({ type: 'report', data: event.data });
        parent?.send({ type: 'unknown', data: event.data });
        machine.state = 'only';
      }
    }
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'only',
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
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createBoss({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  const states = {
    idle: {
      start() {
        machine.context.worker?.stop();
        machine.context = { ...machine.context, worker: createReporter({ parent: machine }).start() };
        machine.state = 'idle';
      },
      report() {
        machine.state = 'idle';
      }
    }
  };
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
      if(machine.context.worker) {
        machine.context.worker.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}

export function createCard({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    shown: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'shown',
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
      listeners.clear();
    }
  };
  return machine;
}

export function createFree({ context = {} } = {}) {
  const listeners = new Set();
  const states = {
    only: {}
  };
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'only',
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
      listeners.clear();
    }
  };
  return machine;
}

export function createBoard({ context = {} } = {}) {
  const listeners = new Set();
  let busy = false;
  const queue = [];
  let waitingChild;
  const states = {
    idle: {
      full(event) {
        machine.context.a?.stop();
        machine.context = {
          ...machine.context,
          a: createCard({
            context: {
              title: event.data,
              owner: machine
            }
          }).start()
        };
        machine.state = 'idle';
      },
      short(event) {
        machine.context.b?.stop();
        machine.context = {
          ...machine.context,
          b: createCard({
            context: {
              title: event.data
            }
          }).start()
        };
        machine.state = 'idle';
      },
      none() {
        machine.context.c?.stop();
        machine.context = { ...machine.context, c: createCard().start() };
        machine.state = 'idle';
      },
      extra(event) {
        machine.context.d?.stop();
        machine.context = {
          ...machine.context,
          d: createCard({
            context: {
              title: event.data,
              owner: machine,
              colour: event.data
            }
          }).start()
        };
        machine.state = 'idle';
      },
      unchecked() {
        machine.context.e?.stop();
        machine.context = {
          ...machine.context,
          e: createFree({
            context: {
              anything: machine
            }
          }).start()
        };
        machine.state = 'idle';
      },
      wait() {
        enterWaiting();
      }
    },
    waiting: {}
  };
  function enterWaiting() {
    machine.state = 'waiting';
    const child = createCard();
    waitingChild = child;
    child.start();
  }
  function exitWaiting() {
    waitingChild.stop();
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
      if(waitingChild) {
        waitingChild.stop();
      }
      if(machine.context.a) {
        machine.context.a.stop();
      }
      if(machine.context.b) {
        machine.context.b.stop();
      }
      if(machine.context.c) {
        machine.context.c.stop();
      }
      if(machine.context.d) {
        machine.context.d.stop();
      }
      if(machine.context.e) {
        machine.context.e.stop();
      }
      queue.length = 0;
      listeners.clear();
    }
  };
  return machine;
}
