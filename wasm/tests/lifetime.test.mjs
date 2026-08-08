// The failure mode this binding is shaped to avoid: JS has no destructors and
// embind has no keep-alive, so `HtmlView`'s bare pointer into its service would
// dangle if a view were ever handed out. Nothing escapes but an integer, and
// every case here has to end in an error rather than a crash.

import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, minimalOdt } from './helper.mjs';

describe('lifetimes', () => {
  let odr;
  before(async () => {
    odr = await Odr();
  });
  after(() => odr.closeAll());

  it('refuses a handle that has been closed', () => {
    const doc = odr.open(minimalOdt());
    doc.render(0);
    doc.close();

    for (const call of [
      () => doc.render(0),
      () => doc.listViews(),
      () => doc.meta(),
      () => doc.read('document.html'),
      () => doc.capabilities(),
    ]) {
      assert.throws(call, (e) => {
        assert.ok(e instanceof OdrError);
        assert.match(e.message, /no such document handle/);
        return true;
      });
    }
  });

  it('closes twice without complaint, and says so', () => {
    const doc = odr.open(minimalOdt());
    assert.equal(doc.close(), true);
    assert.equal(doc.close(), false);
  });

  it('never hands out handle 0, so a zeroed handle is always invalid', () => {
    const doc = odr.open(minimalOdt());
    try {
      assert.ok(doc.handle > 0);
    } finally {
      doc.close();
    }
  });

  it('survives the worker boundary, because a handle is a number', () => {
    const doc = odr.open(minimalOdt('across the wire'));
    try {
      // `structuredClone` is what `postMessage` does to a value.
      assert.equal(structuredClone(doc.handle), doc.handle);

      // The wrapper does not make the trip, and — the trap — it does not fail
      // loudly either: its state is in private fields, which clone away to an
      // empty object. Post the handle, never the `Document`.
      assert.deepEqual(structuredClone(doc), {});
    } finally {
      doc.close();
    }
  });

  it('keeps documents independent', () => {
    const a = odr.open(minimalOdt('first'));
    const b = odr.open(minimalOdt('second'));
    try {
      assert.notEqual(a.handle, b.handle);
      a.close();
      // closing one must not disturb the other
      assert.match(b.render(0).html, /second/);
    } finally {
      b.close();
    }
  });

  it('releases everything on closeAll', () => {
    const doc = odr.open(minimalOdt());
    odr.closeAll();
    assert.throws(() => doc.render(0), OdrError);
  });

  it('closes through Symbol.dispose, so `using` works', () => {
    const doc = odr.open(minimalOdt());
    doc[Symbol.dispose]();
    assert.throws(() => doc.render(0), OdrError);
  });
});
