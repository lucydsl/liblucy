import { createMachine, assign, spawn } from 'xstate';

export default function({ context = {}, services } = {}) {
  return createMachine({
    context,
    states: {
      init: {
        always: [
          {
            target: 'idle',
            actions: [
              'spawnExternal'
            ]
          }
        ]
      },
      idle: {

      }
    }
  }, {
    actions: {
      spawnExternal: assign({
        mac: spawn(services.external, 'external')
      })
    }
  });
}
