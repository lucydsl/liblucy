export default function createMachine({ context = {}, services } = {}) {
  const listeners = new Set();
  const states = {
    init: {},
    idle: {}
  };
  function enterInit() {
    machine.state = 'init';
    machine.context = { ...machine.context, mac: services.external().start() };
    machine.state = 'idle';
  }
  function notify() {
    for(const listener of listeners) {
      listener(machine);
    }
  }
  const machine = {
    state: 'init',
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
      enterInit();
      notify();
      return machine;
    },
    stop() {
      if(machine.context.mac) {
        machine.context.mac.stop();
      }
      listeners.clear();
    }
  };
  return machine;
}
