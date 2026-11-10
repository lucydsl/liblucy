#!/usr/bin/env node
// Runs the behavioural tests in test/behavior/: each compiles a Lucy
// program with lucyc (bin/lucyc, or $LUCYC), imports the module it
// produced, and drives the machine, checking states, context, the order
// actions and guards run in, timers, invokes, spawns and notifications.
//
// A test module exports `source` (the Lucy program), optionally `modules`
// (the files it imports, as { './name.js': text }), and a default function
// run({ mod, helpers, log, clock, assert }), where mod is the compiled
// module, helpers has each of those files' exports by name, log is an
// array the helper modules push to (they import it from './log.js'), and
// clock drives the fake setTimeout.
//
// Usage: scripts/test_behavior.mjs [name ...]
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const lucyc = process.env.LUCYC || 'bin/lucyc';
const dir = 'test/behavior';
const wanted = process.argv.slice(2);
const names = readdirSync(dir)
  .filter(name => name.endsWith('.mjs') && (wanted.length === 0 || wanted.includes(name.replace(/\.mjs$/, ''))))
  .sort();

// setTimeout and clearTimeout that run on a clock the test advances.
function installClock() {
  const real = { setTimeout: globalThis.setTimeout, clearTimeout: globalThis.clearTimeout };
  const timers = new Map();
  let now = 0;
  let nextId = 0;
  globalThis.setTimeout = (fn, ms) => {
    nextId++;
    timers.set(nextId, { at: now + (ms || 0), fn });
    return nextId;
  };
  globalThis.clearTimeout = id => {
    timers.delete(id);
  };
  const clock = {
    // Advance the clock, running every timer due on the way, in order.
    tick(ms) {
      const until = now + ms;
      for(;;) {
        let dueId = null;
        for(const [id, timer] of timers) {
          if(timer.at <= until && (dueId === null || timer.at < timers.get(dueId).at)) {
            dueId = id;
          }
        }
        if(dueId === null) {
          break;
        }
        const timer = timers.get(dueId);
        timers.delete(dueId);
        now = timer.at;
        timer.fn();
      }
      now = until;
    },
    get pending() {
      return timers.size;
    },
    restore() {
      globalThis.setTimeout = real.setTimeout;
      globalThis.clearTimeout = real.clearTimeout;
    }
  };
  return clock;
}

let failed = 0;
for(const name of names) {
  const test = await import(pathToFileURL(resolve(dir, name)));
  const work = mkdtempSync(join(tmpdir(), 'lucy-behavior-'));
  const clock = installClock();
  try {
    writeFileSync(join(work, 'machine.lucy'), test.source);
    writeFileSync(join(work, 'log.js'), 'export const log = [];\n');
    for(const [file, text] of Object.entries(test.modules || {})) {
      writeFileSync(join(work, file), text);
    }
    // Warnings go to stdout; show them, as a test shouldn't have any.
    const warnings = execFileSync(lucyc, ['--out-file', join(work, 'machine.js'), join(work, 'machine.lucy')], { stdio: ['ignore', 'pipe', 'inherit'] });
    if(warnings.length > 0) {
      throw new Error(`lucyc warned:\n${warnings}`);
    }
    const mod = await import(pathToFileURL(join(work, 'machine.js')));
    const { log } = await import(pathToFileURL(join(work, 'log.js')));
    const helpers = {};
    for(const file of Object.keys(test.modules || {})) {
      helpers[file] = await import(pathToFileURL(join(work, file)));
    }
    await test.default({ mod, helpers, log, clock, assert });
    console.log(`ok - ${name}`);
  } catch(err) {
    failed++;
    console.log(`FAILED - ${name}`);
    console.log(err && err.stack ? err.stack : String(err));
  } finally {
    clock.restore();
    rmSync(work, { recursive: true, force: true });
  }
}

process.exit(failed ? 1 : 0);
