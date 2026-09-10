// The ergonomic layer over the embind surface: unwraps `{ok, value | error}`
// envelopes into exceptions and wraps handles in a `Document`. See
// `wasm/AGENTS.md` for why the binding itself does neither.

import createOdrModule from './odr-core.mjs';

export class OdrError extends Error {
  /// `detail` carries `code` (`odr::ErrorCode`) and, for an unsupported type,
  /// `fileType`.
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

  // The name passed to `open`; empty where none was.
  get fileName() {
    return unwrap(this.#core.fileName(this.#handle));
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

  // `capabilities()` narrowed to this document.
  isEditable() {
    return unwrap(this.#core.isEditable(this.#handle));
  }

  isSavable(encrypted = false) {
    return unwrap(this.#core.isSavable(this.#handle, encrypted));
  }

  // Applies what the rendered page's `odr.generateDiff()` collected.
  edit(diff) {
    unwrap(this.#core.edit(this.#handle, diff));
    return this;
  }

  // Structural edits, by the id the render wrote into the page as
  // `data-odr-id`. Each returns the id of what it created; nothing crosses as
  // a handle. `edit()` above is the same operations as one batch.
  removeElement(id) {
    unwrap(this.#core.removeElement(this.#handle, id));
    return this;
  }

  insertTextBefore(anchorId, text) {
    return unwrap(this.#core.insertTextBefore(this.#handle, anchorId, text));
  }

  insertTextAfter(anchorId, text) {
    return unwrap(this.#core.insertTextAfter(this.#handle, anchorId, text));
  }

  appendText(parentId, text) {
    return unwrap(this.#core.appendText(this.#handle, parentId, text));
  }

  // `afterId` of 0 splits before every child.
  splitParagraph(paragraphId, afterId = 0) {
    return unwrap(this.#core.splitParagraph(this.#handle, paragraphId, afterId));
  }

  mergeParagraphWithNext(paragraphId) {
    unwrap(this.#core.mergeParagraphWithNext(this.#handle, paragraphId));
    return this;
  }

  insertParagraphAfter(paragraphId) {
    return unwrap(this.#core.insertParagraphAfter(this.#handle, paragraphId));
  }

  // Whether this pdf can take annotations.
  isAnnotatable() {
    return unwrap(this.#core.isAnnotatable(this.#handle));
  }

  // The annotated pdf's bytes; what `odr.annotation.getAnnotations()` collected
  // goes in.
  annotate(annotations) {
    return unwrap(this.#core.annotate(this.#handle, annotations));
  }

  // The document's bytes, not the rendered html.
  save(password) {
    return password === undefined
      ? unwrap(this.#core.save(this.#handle))
      : unwrap(this.#core.saveEncrypted(this.#handle, password));
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

  // `name` lets a signature-less format like markdown be detected.
  detect(bytes, name = '') {
    return unwrap(this.#core.detect(bytes, name));
  }

  // `fileType` forces an interpretation instead of detecting one; `name` is as
  // in `detect`.
  open(bytes, { fileType, name = '', ...config } = {}) {
    const handle =
      fileType === undefined
        ? unwrap(this.#core.open(bytes, name, config))
        : unwrap(this.#core.openAs(bytes, name, fileType, config));
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
