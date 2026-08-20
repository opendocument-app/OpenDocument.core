// Test plumbing. Inputs are built in memory wherever the assertion allows it,
// following `python/AGENTS.md`; `testfixtures/` holds only the two that cannot
// be — a document with real layout, and an encrypted one.

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateRawSync } from 'node:zlib';

const here = dirname(fileURLToPath(import.meta.url));

// `ODR_WASM_DIST` is set by ctest; the fallback is where a by-hand cmake build
// puts it.
export const dist = process.env.ODR_WASM_DIST ?? join(here, '..', '..', 'dist');

// A static `export ... from` needs a literal specifier, and the package's
// location is only known at run time, so the module is loaded once up front.
const pkg = await import(`${dist}/index.js`);

export const { OdrError, Document } = pkg;

export async function Odr() {
  return pkg.Odr.load();
}

export function fixture(name) {
  return new Uint8Array(readFileSync(join(here, '..', 'testfixtures', name)));
}

const crcTable = (() => {
  const table = new Int32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    table[i] = c;
  }
  return table;
})();

function crc32(buffer) {
  let c = -1;
  for (const byte of buffer) {
    c = crcTable[(c ^ byte) & 0xff] ^ (c >>> 8);
  }
  return (c ^ -1) >>> 0;
}

// Built by hand, so the tests carry no packaging dependency. `store: true`
// writes an entry uncompressed, which ODF requires of `mimetype`.
function zip(entries) {
  const locals = [];
  const centrals = [];
  let offset = 0;

  for (const { name, data, store = false } of entries) {
    const raw = Buffer.from(data);
    const body = store ? raw : deflateRawSync(raw);
    const nameBytes = Buffer.from(name, 'utf8');
    const method = store ? 0 : 8;

    const local = Buffer.alloc(30 + nameBytes.length);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(method, 8);
    local.writeUInt32LE(crc32(raw), 14);
    local.writeUInt32LE(body.length, 18);
    local.writeUInt32LE(raw.length, 22);
    local.writeUInt16LE(nameBytes.length, 26);
    nameBytes.copy(local, 30);
    locals.push(local, body);

    const central = Buffer.alloc(46 + nameBytes.length);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(method, 10);
    central.writeUInt32LE(crc32(raw), 16);
    central.writeUInt32LE(body.length, 20);
    central.writeUInt32LE(raw.length, 24);
    central.writeUInt16LE(nameBytes.length, 28);
    central.writeUInt32LE(offset, 42);
    nameBytes.copy(central, 46);
    centrals.push(central);

    offset += local.length + body.length;
  }

  const directory = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(directory.length, 12);
  end.writeUInt32LE(offset, 16);

  return new Uint8Array(Buffer.concat([...locals, directory, end]));
}

// The smallest odt that renders: one paragraph carrying `text`.
export function minimalOdt(text = 'hello') {
  const mimetype = 'application/vnd.oasis.opendocument.text';
  return zip([
    { name: 'mimetype', data: mimetype, store: true },
    {
      name: 'META-INF/manifest.xml',
      data:
        '<?xml version="1.0" encoding="UTF-8"?>' +
        '<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0" manifest:version="1.2">' +
        `<manifest:file-entry manifest:full-path="/" manifest:media-type="${mimetype}"/>` +
        '<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>' +
        '</manifest:manifest>',
    },
    {
      name: 'content.xml',
      data:
        '<?xml version="1.0" encoding="UTF-8"?>' +
        '<office:document-content' +
        ' xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"' +
        ' xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"' +
        ' office:version="1.2">' +
        '<office:body><office:text>' +
        `<text:p>${text}</text:p>` +
        '</office:text></office:body></office:document-content>',
    },
  ]);
}
