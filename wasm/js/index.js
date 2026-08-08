// The ergonomic layer over the embind surface: unwraps `{ok, value | error}`
// envelopes into exceptions and wraps handles in a `Document`. See
// `wasm/AGENTS.md` for why the binding itself does neither.

import createOdrModule from './odr-core.mjs';

export class OdrError extends Error {
  constructor(type, message, detail) {
    super(message);
    this.name = type;
    Object.assign(this, detail);
  }
}

function unwrap(envelope) {
  if (envelope.ok) {
    return envelope.value;
  }
  const { type, message, ...detail } = envelope.error;
  throw new OdrError(type, message, detail);
}

// Holds a handle into the wasm heap, so it must be closed: JS has no
// destructors and the module cannot know when you are done.
export class Document {
  #core;
  #handle;

  constructor(core, handle) {
    this.#core = core;
    this.#handle = handle;
  }

  // Pass this across `postMessage` rather than the object, which does not
  // survive structured cloning.
  get handle() {
    return this.#handle;
  }

  get fileType() {
    return unwrap(this.#core.fileType(this.#handle));
  }

  meta() {
    return JSON.parse(unwrap(this.#core.meta(this.#handle)));
  }

  capabilities() {
    return unwrap(this.#core.capabilities(this.#handle));
  }

  isPasswordEncrypted() {
    return unwrap(this.#core.isPasswordEncrypted(this.#handle));
  }

  // Anything already rendered is discarded, having come from the encrypted file.
  decrypt(password) {
    unwrap(this.#core.decrypt(this.#handle, password));
    return this;
  }

  listViews() {
    return unwrap(this.#core.listViews(this.#handle));
  }

  render(index = 0) {
    return unwrap(this.#core.renderView(this.#handle, index));
  }

  read(path) {
    return unwrap(this.#core.readPath(this.#handle, path));
  }

  close() {
    return unwrap(this.#core.close(this.#handle));
  }

  [Symbol.dispose]() {
    this.close();
  }
}

export class Odr {
  #core;

  constructor(core) {
    this.#core = core;
    this.enums = core.enumTables();
  }

  static async load(moduleOptions) {
    return new Odr(await createOdrModule(moduleOptions));
  }

  version() {
    return this.#core.version();
  }

  identify() {
    return this.#core.identify();
  }

  fileTypes() {
    return this.#core.fileTypes();
  }

  detect(bytes) {
    return unwrap(this.#core.detect(bytes));
  }

  // `fileType` forces an interpretation instead of detecting one.
  open(bytes, { fileType, ...config } = {}) {
    const handle =
      fileType === undefined
        ? unwrap(this.#core.open(bytes, config))
        : unwrap(this.#core.openAs(bytes, fileType, config));
    return new Document(this.#core, handle);
  }

  setLogger(sink, level = 2) {
    unwrap(this.#core.setLogger(sink, level));
  }

  // The escape hatch for a worker being torn down; prefer closing individually.
  closeAll() {
    unwrap(this.#core.closeAll());
  }
}

export default Odr;
