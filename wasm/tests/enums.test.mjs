// Enums cross by ordinal, and the headers say to append and never reorder
// (`src/odr/html.hpp`, `src/odr/file.hpp`). `FileType`, `FileCategory` and
// `DocumentType` are derived from the library's tables and cannot drift; the
// rest have none, so they are pinned here. Appending stays silent by design;
// reordering fails here rather than silently in a consumer months later.

import assert from 'node:assert/strict';
import { before, describe, it } from 'node:test';

import { Odr } from './helper.mjs';

const pinned = {
  HtmlResourceType: {
    html_fragment: 0,
    css: 1,
    js: 2,
    image: 3,
    font: 4,
    media: 5,
  },
  HtmlColorScheme: { light: 0, dark: 1, system: 2 },
  HtmlTableGridlines: { none: 0, soft: 1, hard: 2 },
  HtmlViewportMode: {
    automatic: 0,
    fit_width: 1,
    actual_size: 2,
    none: 3,
    fit_width_by_view: 4,
  },
  PdfTextMode: { dual_layer: 0, single_layer: 1 },
  EncryptionState: {
    unknown: 0,
    not_encrypted: 1,
    encrypted: 2,
    decrypted: 3,
  },
  LogLevel: {
    verbose: 0,
    debug: 1,
    info: 2,
    warning: 3,
    error: 4,
    fatal: 5,
  },
};

describe('enums', () => {
  let enums;
  before(async () => {
    enums = (await Odr()).enums;
  });

  for (const [name, expected] of Object.entries(pinned)) {
    it(`${name} keeps its ordinals`, () => {
      for (const [key, ordinal] of Object.entries(expected)) {
        assert.equal(
          enums[name][key],
          ordinal,
          `${name}.${key} moved from ${ordinal} to ${enums[name][key]}`,
        );
      }
    });
  }

  it('derives TextEncoding from the library, unknown first', () => {
    assert.equal(enums.TextEncoding.unknown, 0);
    // keyed by the canonical name, as `FileType` is keyed by `odt`
    assert.equal(enums.TextEncoding['UTF-8'], 1);
    // a named-but-not-decodable one is still in the table
    assert.equal(typeof enums.TextEncoding['Shift_JIS'], 'number');
  });

  it('derives FileType from the library, unknown first', () => {
    assert.equal(enums.FileType.unknown, 0);
    assert.equal(typeof enums.FileType.odt, 'number');
    assert.equal(typeof enums.FileType.docx, 'number');
    assert.equal(typeof enums.FileType.pdf, 'number');
  });

  it('derives FileCategory and DocumentType too', () => {
    assert.equal(enums.FileCategory.unknown, 0);
    assert.equal(enums.DocumentType.unknown, 0);
    assert.equal(typeof enums.DocumentType.text, 'number');
    assert.equal(typeof enums.FileCategory.document, 'number');
  });

  // The head keeps the order `ODRError` shipped, so an installed iOS app does
  // not read a moved number; the edit band is what the rendered page raises.
  it('derives ErrorCode, both bands pinned', () => {
    assert.equal(enums.ErrorCode.Unknown, 1);
    assert.equal(enums.ErrorCode.UnsupportedFileType, 5);
    assert.equal(enums.ErrorCode.WrongPassword, 11);
    assert.equal(enums.ErrorCode.DocumentCopyProtected, 15);

    assert.equal(enums.ErrorCode.newLine, 1001);
    assert.equal(enums.ErrorCode.formula, 1002);
    assert.equal(enums.ErrorCode.readOnly, 1005);
    assert.equal(enums.ErrorCode.formulaInput, 1006);
    assert.equal(enums.ErrorCode.unsupportedEdit, 1007);
    assert.equal(enums.ErrorCode.range, 1008);
    assert.equal(enums.ErrorCode.unnameableEdit, 1009);
  });
});
