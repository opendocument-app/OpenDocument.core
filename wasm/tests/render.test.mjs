import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';

import { Odr, OdrError, fixture, minimalOdt } from './helper.mjs';

describe('render', () => {
  let odr;
  before(async () => {
    odr = await Odr();
  });
  after(() => odr.closeAll());

  it('renders a view to self-contained html', () => {
    const doc = odr.open(fixture('mixed-layout.odt'));
    try {
      const views = doc.listViews();
      assert.equal(views.length, 1);
      assert.deepEqual(views[0], {
        name: 'document',
        index: 0,
        path: 'document.html',
      });

      const { html, externalResources } = doc.render(0);
      assert.match(html, /^<!DOCTYPE html>/);
      assert.match(html, /<style>/);
      assert.match(html, /window\.odr|var odr/);

      // The point of the default config: nothing to fetch, so the html can go
      // straight into a `blob:` iframe.
      assert.deepEqual(externalResources, []);
      for (const [, src] of html.matchAll(/<img[^>]+src="([^"]*)"/g)) {
        assert.ok(
          src.startsWith('data:'),
          `image src is not inline, so a blob: iframe would 404 on it: ${src}`,
        );
      }
    } finally {
      doc.close();
    }
  });

  it('renders bytes built in memory, with no file involved', () => {
    const doc = odr.open(minimalOdt('hello from a buffer'));
    try {
      assert.match(doc.render(0).html, /hello from a buffer/);
    } finally {
      doc.close();
    }
  });

  it('reads a view back through the virtual filesystem', () => {
    const doc = odr.open(fixture('mixed-layout.odt'));
    try {
      const rendered = doc.render(0).html;
      const { bytes, mimeType } = doc.read('document.html');

      assert.equal(mimeType, 'text/html');
      assert.ok(bytes instanceof Uint8Array);
      assert.equal(Buffer.from(bytes).toString('utf8'), rendered);
    } finally {
      doc.close();
    }
  });

  it('reports an unknown path rather than returning empty bytes', () => {
    const doc = odr.open(fixture('mixed-layout.odt'));
    try {
      assert.throws(() => doc.read('nope.html'), (e) => {
        assert.ok(e instanceof OdrError);
        assert.equal(e.name, 'FileNotFound');
        return true;
      });
    } finally {
      doc.close();
    }
  });

  // The C++ suite covers where the floor lands; this only proves the sides
  // cross the binding as css lengths, omitted ones staying omitted.
  it('honours a minimum content margin', () => {
    const plain = odr.open(fixture('mixed-layout.odt'));
    const inset = odr.open(fixture('mixed-layout.odt'), {
      minContentMargin: { top: '12px', left: '1cm' },
    });
    try {
      assert.ok(!plain.render(0).html.includes(':root{--odr-min-margin'));
      const html = inset.render(0).html;
      assert.ok(
        html.includes(':root{--odr-min-margin-top:12px;--odr-min-margin-left:1cm;}'),
      );
      assert.ok(!html.includes('--odr-min-margin-right:'));
    } finally {
      plain.close();
      inset.close();
    }
  });

  it('honours a config passed at open', () => {
    const plain = odr.open(fixture('mixed-layout.odt'));
    const editable = odr.open(fixture('mixed-layout.odt'), { editable: true });
    try {
      assert.ok(!plain.render(0).html.includes('contenteditable'));
      assert.ok(editable.render(0).html.includes('contenteditable'));
    } finally {
      plain.close();
      editable.close();
    }
  });
});
