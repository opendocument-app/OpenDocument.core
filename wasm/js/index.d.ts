/** Hand-written, because the package ships plain JavaScript — see `wasm/AGENTS.md`. */

/** Ordinals, mirroring the C++ enums. Read them from `Odr.enums`, never inline
 * a number: the headers guarantee only that values are appended. */
export interface EnumTables {
  FileType: Record<string, number>;
  FileCategory: Record<string, number>;
  DocumentType: Record<string, number>;
  HtmlResourceType: Record<string, number>;
  HtmlColorScheme: Record<string, number>;
  HtmlTableGridlines: Record<string, number>;
  HtmlViewportMode: Record<string, number>;
  PdfTextMode: Record<string, number>;
  EncryptionState: Record<string, number>;
  LogLevel: Record<string, number>;
}

export interface Capabilities {
  detectByContent: boolean;
  open: boolean;
  decrypt: boolean;
  translateHtml: boolean;
  /** The view this type renders as honors `HtmlConfig.colorScheme`. */
  colorScheme: boolean;
  edit: boolean;
  save: boolean;
  encrypt: boolean;
}

export interface FileTypeInfo {
  fileType: number;
  name: string;
  category: number;
  documentType: number;
  extensions: string[];
  mimeTypes: string[];
  capabilities: Capabilities;
}

export interface Detection {
  /** Most specific last: a zip names the container first, its contents after. */
  fileTypes: number[];
  mimeType: string;
}

/** What a view leaves out of the sheet it renders. */
export interface SheetCut {
  /** The extent the sheet's cells span. */
  contentRows: number;
  contentColumns: number;
  /** The extent the markup carries. */
  renderedRows: number;
  renderedColumns: number;
}

export interface View {
  name: string;
  index: number;
  path: string;
  /** Set where `spreadsheetLimit` or `spreadsheetCellLimit` cut this view's sheet. */
  sheetCut?: SheetCut;
}

/** A resource the markup links to rather than inlining. Fetch with
 * {@link Document.read}. Empty unless the document carries media, or
 * `embedImages` was turned off. */
export interface ExternalResource {
  path: string;
  mimeType: string;
  type: number;
}

export interface Rendered {
  html: string;
  externalResources: ExternalResource[];
}

export interface Content {
  bytes: Uint8Array;
  mimeType: string;
}

/** Anything omitted keeps the library's default. */
export interface HtmlConfig {
  embedImages?: boolean;
  editable?: boolean;
  textDocumentMargin?: boolean;
  formatHtml?: boolean;
  /** @deprecated Inert. */
  embedOutline?: boolean;
  /** @deprecated Inert. */
  noDrm?: boolean;
  /** @deprecated Inert. */
  backgroundImageFormat?: string;
  /** @deprecated Inert. */
  backgroundImageDpi?: number;
  pageRangeBegin?: number;
  pageRangeEnd?: number;
  colorScheme?: number;
  /** Largest sheet region written, per axis; `null` drops the cap. */
  spreadsheetLimit?: { rows: number; columns: number } | null;
  /** Most cells written for one sheet; `null` drops the budget. */
  spreadsheetCellLimit?: number | null;
  spreadsheetGridlines?: number;
  viewportMode?: number;
  /** The width the output is shown at, in css pixels; fits paged content to it. */
  viewportWidth?: number;
  /** The zoom the view opens at, 1 being actual size; unset follows the fit. */
  initialZoom?: number;
  /**
   * The least distance the generated content keeps from the view's border, per
   * side, as a css length (e.g. `"3mm"`). A given side raises the inset the
   * view already has, never lowers it.
   */
  minContentMargin?: {
    top?: string;
    right?: string;
    bottom?: string;
    left?: string;
  };
  pdfTextMode?: number;
}

export interface OpenOptions extends HtmlConfig {
  /** Force an interpretation instead of detecting one. */
  fileType?: number;
  /** What the upload was called. Bytes carry no name, and for a format with no
   * signature — markdown — it is the only thing that can offer the type. */
  name?: string;
}

/** `name` is the C++ exception type: `WrongPassword`, `UnsupportedFileType`, … */
export declare class OdrError extends Error {
  /** Set when `name` is `UnsupportedFileType`. */
  fileType?: number;
}

export declare class Document {
  /** Pass this across `postMessage`, never the `Document` — the wrapper's state
   * is in private fields and clones away to an empty object. */
  readonly handle: number;
  readonly fileType: number;
  /** What the bytes were called, as passed to `open`; empty where nothing was. */
  readonly fileName: string;

  meta(): Record<string, unknown>;
  capabilities(): Capabilities;
  isPasswordEncrypted(): boolean;
  /** @throws OdrError `WrongPassword` */
  decrypt(password: string): this;

  listViews(): View[];
  /** With the default `embedImages`, `html` is self-contained and can go
   * straight into a `blob:` iframe. */
  render(index?: number): Rendered;
  read(path: string): Content;

  /** `capabilities()` narrowed to this document. */
  isEditable(): boolean;
  isSavable(encrypted?: boolean): boolean;
  /** Applies what the rendered page's `odr.generateDiff()` collected.
   * @throws OdrError `NoDocumentFile` */
  edit(diff: string): this;
  /** The document's bytes, not the rendered html.
   * @throws OdrError `UnsupportedOperation` where the format cannot be saved */
  save(password?: string): Uint8Array;

  /** Idempotent; returns whether it released anything. */
  close(): boolean;
  [Symbol.dispose](): void;
}

export declare class Odr {
  readonly enums: EnumTables;

  static load(moduleOptions?: Record<string, unknown>): Promise<Odr>;

  version(): string;
  /** Version, commit and dirty flag. Reads "unknown version" on an unreleased
   * build, which is correct — `main` carries none. */
  identify(): string;
  /** Every known type, enough to populate an `<input accept>` or a PWA
   * manifest's file handlers without opening anything. */
  fileTypes(): FileTypeInfo[];
  detect(bytes: Uint8Array, name?: string): Detection;
  open(bytes: Uint8Array, options?: OpenOptions): Document;
  /** Applies to documents opened after the call. Null silences it again. */
  setLogger(sink: ((level: number, message: string) => void) | null, level?: number): void;
  /** Releases every open document; prefer closing them individually. */
  closeAll(): void;
}

export default Odr;
