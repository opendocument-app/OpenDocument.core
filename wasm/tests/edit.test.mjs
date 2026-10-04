// The round trip: render editable, apply the page's diff, save the bytes back.

import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, minimalOds, minimalOdt, minimalPdf } from './helper.mjs';

// Read out of the html rather than spelled, as the browser does. The runs
// carry the ids an op names; a paragraph carries one too, so the tag counts.
function firstEditableRunId(html) {
  const match = html.match(/<x-s [^>]*data-odr-id="(\d+)"/);
  assert.ok(match, 'the editable render carries no run id');
  return Number(match[1]);
}

function firstEditableParagraphId(html) {
  const match = html.match(/<x-p [^>]*data-odr-id="(\d+)"/);
  assert.ok(match, 'the editable render carries no paragraph id');
  return Number(match[1]);
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
      const id = firstEditableRunId(html);
      doc.edit(JSON.stringify({
        version: 2,
        ops: [{ op: 'setText', id, text: 'edited in the browser' }],
      }));

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

  it('writes a sheet cell by position and saves it', () => {
    const doc = odr.open(minimalOds('hello'));
    try {
      doc.edit(JSON.stringify({
        version: 2,
        ops: [{
          op: 'setCell', sheet: 0, column: 0, row: 0,
          value: { type: 'number', number: 12.5, text: '12.5' },
        }],
      }));
      assert.match(doc.render(0).html, /12\.5/);

      const reopened = odr.open(doc.save());
      try {
        assert.match(reopened.render(0).html, /12\.5/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('styles a sheet cell by position and saves the style', () => {
    const doc = odr.open(minimalOds('hello'));
    try {
      doc.setCellStyle(0, 0, 0, { fill: '#ffff00', bold: true });
      assert.match(doc.render(0).html, /background-color:#ffff00/);

      const reopened = odr.open(doc.save());
      try {
        const html = reopened.render(0).html;
        assert.match(html, /background-color:#ffff00/);
        assert.match(html, /font-weight:bold/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('answers null for a document stating no locale', () => {
    const doc = odr.open(minimalOds('hello'));
    try {
      assert.equal(doc.locale(), null);
    } finally {
      doc.close();
    }
  });

  it('styles a whole row and a whole column', () => {
    const doc = odr.open(minimalOds('hello'));
    try {
      doc.setRowStyle(0, 0, { bold: true }).setColumnStyle(0, 3, { fill: '#00ff00' });
      const reopened = odr.open(doc.save());
      try {
        assert.match(reopened.render(0).html, /font-weight:bold/);
      } finally {
        reopened.close();
      }
      assert.throws(() => doc.setRowStyle(0, 0, 'bold'), OdrError);
    } finally {
      doc.close();
    }
  });

  it('refuses a cell style it cannot write', () => {
    const doc = odr.open(minimalOds('hello'));
    try {
      assert.throws(() => doc.setCellStyle(0, 0, 0, { highlight: '#ffff00' }), OdrError);
      assert.throws(() => doc.setCellStyle(0, 0, 0, 'bold'), OdrError);
    } finally {
      doc.close();
    }
  });

  it('edits structurally by id and saves the result', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const id = firstEditableRunId(doc.render(0).html);

      assert.equal(typeof doc.insertTextBefore(id, 'before '), 'number');
      assert.equal(typeof doc.insertTextAfter(id, ' after'), 'number');

      const paragraph = doc.insertParagraphAfter(
        Number(doc.render(0).html.match(/<x-p [^>]*data-odr-id="(\d+)"/)[1]));
      doc.appendText(paragraph, 'a new paragraph');

      const reopened = odr.open(doc.save());
      try {
        const html = reopened.render(0).html;
        assert.match(html, /before /);
        assert.match(html, /a new paragraph/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('marks a run by id and saves the mark', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const id = firstEditableRunId(doc.render(0).html);
      doc.setTextStyle(id, { bold: true, highlight: '#ffff00' });
      assert.match(doc.render(0).html, /font-weight:bold/);

      const reopened = odr.open(doc.save());
      try {
        const html = reopened.render(0).html;
        assert.match(html, /font-weight:bold/);
        assert.match(html, /background-color:#ffff00/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('refuses a style property it does not know', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const id = firstEditableRunId(doc.render(0).html);
      assert.throws(() => doc.setTextStyle(id, { blink: true }), OdrError);
      assert.throws(() => doc.setTextStyle(id, 'bold'), OdrError);
    } finally {
      doc.close();
    }
  });

  it('aligns a paragraph by id and saves the alignment', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const id = firstEditableParagraphId(doc.render(0).html);
      doc.setParagraphStyle(id, { align: 'center' });
      assert.match(doc.render(0).html, /text-align:center/);

      const reopened = odr.open(doc.save());
      try {
        assert.match(reopened.render(0).html, /text-align:center/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });

  it('refuses an alignment it does not know', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      const id = firstEditableParagraphId(doc.render(0).html);
      assert.throws(() => doc.setParagraphStyle(id, { align: 'middle' }), OdrError);
      assert.throws(() => doc.setParagraphStyle(id, 'center'), OdrError);
    } finally {
      doc.close();
    }
  });

  it('removes an element by id', () => {
    const doc = odr.open(minimalOdt('hello'), { editable: true });
    try {
      doc.removeElement(firstEditableRunId(doc.render(0).html));
      assert.doesNotMatch(doc.render(0).html, /hello/);
    } finally {
      doc.close();
    }
  });

  it('refuses an id the document does not hold', () => {
    const doc = odr.open(minimalOdt('hello'));
    try {
      assert.throws(() => doc.removeElement(999999), OdrError);
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

  it('edits and saves a plain text file through the same calls', () => {
    const doc = odr.open(new TextEncoder().encode('lorem ipsum'), {
      editable: true,
      name: 'notes.txt',
    });
    try {
      assert.equal(doc.isEditable(), true);
      assert.equal(doc.isSavable(), true);
      assert.equal(doc.isSavable(true), false);
      assert.equal(new TextDecoder().decode(doc.save()), 'lorem ipsum');

      assert.match(doc.render(0).html, /lorem ipsum/);
      doc.edit(JSON.stringify({
        version: 2,
        ops: [{ op: 'setContent', text: 'edited in the browser' }],
      }));
      assert.equal(doc.fileName, 'notes.txt');
      assert.match(doc.render(0).html, /edited in the browser/);

      const saved = doc.save();
      assert.equal(new TextDecoder().decode(saved), 'edited in the browser');
      const reopened = odr.open(saved);
      try {
        assert.equal(reopened.fileType, odr.enums.FileType.txt);
      } finally {
        reopened.close();
      }

      assert.throws(() => doc.save('secret'), (error) => {
        assert.equal(error.name, 'UnsupportedOperation');
        return true;
      });
    } finally {
      doc.close();
    }
  });

  it('refuses a text file of a type it does not write', () => {
    const doc = odr.open(new TextEncoder().encode('{"a": 1}'), { editable: true });
    try {
      assert.equal(doc.isEditable(), false);
      assert.equal(doc.isSavable(), false);
      assert.match(doc.render(0).html, /data-odr-editable="readOnly"/);
      for (const call of [() => doc.save(), () => doc.edit('{"version":2,"ops":[]}')]) {
        assert.throws(call, (error) => {
          assert.equal(error.name, 'UnsupportedOperation');
          return true;
        });
      }
    } finally {
      doc.close();
    }
  });

  it('refuses a file that is not a document', () => {
    const doc = odr.open(minimalPdf());
    try {
      assert.throws(() => doc.save(), (error) => {
        assert.ok(error instanceof OdrError);
        assert.equal(error.name, 'NoDocumentFile');
        return true;
      });
      assert.throws(() => doc.edit('{"version":2,"ops":[]}'), OdrError);
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

  it('creates a document of every type that states create', () => {
    for (const { fileType, capabilities } of odr.fileTypes()) {
      if (!capabilities.create) {
        continue;
      }
      const doc = odr.create(fileType);
      try {
        assert.equal(doc.fileType, fileType);
        assert.equal(doc.isEditable(), true);
      } finally {
        doc.close();
      }
    }
  });

  it('refuses to create a type that cannot be created', () => {
    assert.throws(() => odr.create(odr.enums.FileType.pptx), (error) => {
      assert.ok(error instanceof OdrError);
      assert.equal(error.name, 'UnsupportedFileType');
      return true;
    });
  });

  it('types into a created document and saves it', () => {
    const doc = odr.create(odr.enums.FileType.odt, { editable: true });
    try {
      const id = firstEditableParagraphId(doc.render(0).html);
      doc.edit(JSON.stringify({
        version: 2,
        ops: [{ op: 'insertText', parent: id, text: 'typed in the browser', id: -1 }],
      }));

      const reopened = odr.open(doc.save());
      try {
        assert.match(reopened.render(0).html, /typed in the browser/);
      } finally {
        reopened.close();
      }
    } finally {
      doc.close();
    }
  });
});
