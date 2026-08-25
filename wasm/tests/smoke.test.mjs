import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, fixture, minimalOdt } from './helper.mjs';

describe('smoke', () => {
  let odr;
  before(async () => {
    odr = await Odr();
  });
  after(() => odr.closeAll());

  it('identifies itself', () => {
    // `main` carries no version, so `version()` is legitimately empty there and
    // only `identify()` is guaranteed to say something.
    assert.equal(typeof odr.version(), 'string');
    assert.match(odr.identify(), /\(.*\)/);
  });

  it('detects a file type from bytes alone', () => {
    const { fileTypes, mimeType } = odr.detect(fixture('mixed-layout.odt'));

    assert.ok(fileTypes.length > 0);
    // a zip names the container first and what is inside it after
    assert.equal(fileTypes.at(-1), odr.enums.FileType.odt);
    assert.equal(mimeType, 'application/vnd.oasis.opendocument.text');
  });

  // Text is the fallback, but only for bytes that read as text.
  it('opens bytes that read as text and refuses the rest', () => {
    const junk = new Uint8Array(512);
    assert.throws(() => odr.detect(junk), OdrError);
    assert.throws(() => odr.open(junk), OdrError);

    const text = new TextEncoder().encode('lorem ipsum dolor sit amet');
    assert.equal(odr.detect(text).mimeType, 'text/plain');

    const doc = odr.open(text);
    try {
      assert.equal(doc.fileType, odr.enums.FileType.txt);
    } finally {
      doc.close();
    }
  });

  it('throws a typed error for a wrong password', () => {
    const doc = odr.open(fixture('encrypted.docx'));
    try {
      assert.equal(doc.isPasswordEncrypted(), true);
      assert.throws(() => doc.decrypt('not the password'), (e) => {
        assert.ok(e instanceof OdrError);
        assert.equal(e.name, 'WrongPassword');
        return true;
      });
      assert.doesNotThrow(() => doc.decrypt('password'));
      assert.ok(doc.render(0).html.length > 0);
    } finally {
      doc.close();
    }
  });

  it('lists file types with the tables a file picker needs', () => {
    const types = odr.fileTypes();
    assert.ok(types.length > 10);

    const odt = types.find((t) => t.name === 'odt');
    assert.ok(odt.extensions.includes('odt'));
    assert.ok(odt.mimeTypes.includes('application/vnd.oasis.opendocument.text'));
    assert.equal(odt.capabilities.translateHtml, true);
    assert.equal(odt.capabilities.colorScheme, true);
    assert.equal(odt.capabilities.open, true);

    // a pdf renders, but paints its own page backgrounds
    const pdf = types.find((t) => t.name === 'pdf');
    assert.equal(pdf.capabilities.translateHtml, true);
    assert.equal(pdf.capabilities.colorScheme, false);

    // both tables are always present, so a caller may concatenate without
    // checking
    for (const type of types) {
      assert.ok(Array.isArray(type.extensions));
      assert.ok(Array.isArray(type.mimeTypes));
    }
  });

  it('answers metadata without rendering', () => {
    const doc = odr.open(minimalOdt());
    try {
      const meta = doc.meta();
      assert.equal(meta.fileType, 'odt');
      assert.equal(meta.documentType, 'text');
      assert.equal(meta.isEncrypted, false);
      assert.equal(doc.isPasswordEncrypted(), false);
      assert.equal(doc.capabilities().translateHtml, true);
    } finally {
      doc.close();
    }
  });

  it('routes logging to a sink and back to silence', () => {
    const records = [];
    odr.setLogger((level, message) => records.push({ level, message }), 0);
    const doc = odr.open(minimalOdt());
    doc.render(0);
    doc.close();
    odr.setLogger(null);

    assert.ok(records.length > 0, 'the sink saw nothing');
    assert.equal(typeof records[0].level, 'number');
    assert.equal(typeof records[0].message, 'string');
  });
});
