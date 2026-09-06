// The round trip the browser drives: mark a pdf up, get the bytes back.

import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, minimalPdf } from './helper.mjs';

const highlight = JSON.stringify({
  version: 1,
  annotations: [
    {
      page: 0,
      type: 'highlight',
      quads: [[72, 700, 300, 700, 72, 688, 300, 688]],
      color: [1, 0.9, 0.2],
    },
  ],
});

describe('annotate', () => {
  let odr;
  before(async () => {
    odr = await Odr();
  });
  after(() => odr.closeAll());

  it('declares the capability', () => {
    const doc = odr.open(minimalPdf());
    try {
      assert.equal(doc.capabilities().annotate, true);
    } finally {
      doc.close();
    }
  });

  it('appends the annotation to the source', () => {
    const source = minimalPdf();
    const doc = odr.open(source);
    try {
      const result = doc.annotate(highlight);

      assert.ok(result.length > source.length);
      // the source is copied through and the annotation written after it
      assert.deepEqual(result.slice(0, source.length), source);

      const text = Buffer.from(result).toString('latin1');
      assert.match(text, /\/Highlight/);
      assert.match(text, /\/Subtype \/Form/);
    } finally {
      doc.close();
    }
  });

  it('refuses a payload it does not understand', () => {
    const doc = odr.open(minimalPdf());
    try {
      assert.throws(() => doc.annotate('{"version": 2}'), OdrError);
      assert.throws(() => doc.annotate('not json'), OdrError);
    } finally {
      doc.close();
    }
  });
});
