// The round trip: render editable, apply the page's diff, save the bytes back.

import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, minimalOdt } from './helper.mjs';

// Read out of the html rather than spelled, as the browser does.
function firstEditablePath(html) {
  const match = html.match(/data-odr-path="([^"]+)"/);
  assert.ok(match, 'the editable render carries no data-odr-path');
  return match[1];
}

describe('edit', () => {
  let odr;
  before(async () => {
    odr = await Odr();
  });
  after(() => odr.closeAll());

  it('reports what this document can do', () => {
    const doc = odr.open(minimalOdt());
    try {
      assert.equal(doc.isEditable(), true);
      assert.equal(doc.isSavable(), true);
      assert.equal(doc.isSavable(true), false);
    } finally {
      doc.close();
    }
  });

  it('applies a diff and saves the document it renders', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const { html } = doc.render(0);
      assert.match(html, /contenteditable/);

      const path = firstEditablePath(html);
      doc.edit(JSON.stringify({ modifiedText: { [path]: 'edited in the browser' } }));

      // the edit is in the document, so the same service renders it
      assert.match(doc.render(0).html, /edited in the browser/);

      const saved = doc.save();
      assert.ok(saved instanceof Uint8Array);
      // a zip, i.e. the document rather than the rendered page
      assert.deepEqual(Array.from(saved.subarray(0, 2)), [0x50, 0x4b]);

      const reopened = odr.open(saved);
      try {
        assert.equal(reopened.fileType, odr.enums.FileType.odt);
        assert.match(reopened.render(0).html, /edited in the browser/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('saves without a render having happened', () => {
    const doc = odr.open(minimalOdt('untouched'));
    try {
      assert.match(new TextDecoder().decode(doc.save()), /^PK/);
    } finally {
      doc.close();
    }
  });

  it('refuses a file that is not a document', () => {
    const doc = odr.open(new TextEncoder().encode('lorem ipsum dolor sit amet'));
    try {
      assert.throws(() => doc.save(), (error) => {
        assert.ok(error instanceof OdrError);
        assert.equal(error.name, 'NoDocumentFile');
        return true;
      });
      assert.throws(() => doc.edit('{"modifiedText":{}}'), OdrError);
    } finally {
      doc.close();
    }
  });

  it('refuses an encrypted save, which no format supports yet', () => {
    const doc = odr.open(minimalOdt());
    try {
      assert.throws(() => doc.save('secret'), (error) => {
        assert.equal(error.name, 'UnsupportedOperation');
        return true;
      });
    } finally {
      doc.close();
    }
  });
});
