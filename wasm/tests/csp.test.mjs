import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, it } from 'node:test';

import { dist } from './helper.mjs';

// `wasm-unsafe-eval` allows `WebAssembly.instantiate` but not `new Function`,
// which embind uses for its invokers unless linked with
// `-sDYNAMIC_EXECUTION=0`.
describe('content security policy', () => {
  it('loads where dynamic code construction is blocked', () => {
    // A child process, because the stand-in below replaces a global the test
    // runner itself uses.
    const script = `
      globalThis.Function = new Proxy(Function, {
        construct() { throw new EvalError('blocked by the stand-in CSP'); },
        apply() { throw new EvalError('blocked by the stand-in CSP'); },
      });
      const { Odr } = await import(${JSON.stringify(join(dist, 'index.js'))});
      await Odr.load();
    `;

    execFileSync(process.execPath, ['--input-type=module', '-e', script], {
      stdio: 'pipe',
    });
  });

  it('ships glue that builds no code at run time', () => {
    const glue = readFileSync(join(dist, 'odr-core.mjs'), 'utf8');

    assert.doesNotMatch(glue, /new Function\b/);
    assert.doesNotMatch(glue, /[^\w.$]eval\(/);
  });
});
