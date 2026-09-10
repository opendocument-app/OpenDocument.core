package app.opendocument.core;

/** A decoded document. Mirrors {@code odr::Document}. */
public final class Document extends NativeResource {
  Document(long handle) {
    super(handle, null, Document::destroy);
  }

  public boolean isEditable() {
    return isEditableNative(handle());
  }

  public boolean isSavable() {
    return isSavable(false);
  }

  public boolean isSavable(boolean encrypted) {
    return isSavableNative(handle(), encrypted);
  }

  public void save(String path) {
    saveNative(handle(), path);
  }

  public void save(String path, String password) {
    saveEncryptedNative(handle(), path, password);
  }

  /** The saved document as bytes. */
  public byte[] saveToMemory() {
    return saveToMemoryNative(handle());
  }

  public byte[] saveToMemory(String password) {
    return saveToMemoryEncryptedNative(handle(), password);
  }

  public FileType fileType() {
    return FileType.fromNative(fileTypeNative(handle()));
  }

  public DocumentType documentType() {
    return DocumentType.fromNative(documentTypeNative(handle()));
  }

  /** Root of the element tree; keeps this document reachable. */
  public Element rootElement() {
    return new Element(rootElementNative(handle()), this);
  }

  public Filesystem asFilesystem() {
    return new Filesystem(asFilesystemNative(handle()), this);
  }

  /** Applies the operations our browser-side editor produces. */
  public void edit(String diff) {
    editNative(handle(), diff);
  }

  /** The element {@link Element#identifier()} handed out, or {@code null}. */
  public Element elementById(long identifier) {
    long h = elementByIdNative(handle(), identifier);
    return h == 0 ? null : new Element(h, this);
  }

  /**
   * Removes an element and its subtree; its identifier stays taken.
   *
   * <p>The structural edits below all throw where the engine cannot write, and
   * for an element of another document.
   */
  public void remove(Element element) {
    removeNative(handle(), element.handle());
  }

  /** A run before another, in the same parent, so it takes the same style. */
  public Text insertTextBefore(Text anchor, String text) {
    return new Text(insertTextBeforeNative(handle(), anchor.handle(), text), this);
  }

  /** A run after another, in the same parent. */
  public Text insertTextAfter(Text anchor, String text) {
    return new Text(insertTextAfterNative(handle(), anchor.handle(), text), this);
  }

  /** A run as the last child of an element. */
  public Text appendText(Element parent, String text) {
    return new Text(appendTextNative(handle(), parent.handle(), text), this);
  }

  /** Splits before every child of the paragraph. */
  public Paragraph splitParagraph(Paragraph paragraph) {
    return splitParagraph(paragraph, null);
  }

  /**
   * Splits a paragraph after {@code after}, one of its descendants, into a new
   * paragraph of the same style. A {@code null} {@code after} moves every child.
   */
  public Paragraph splitParagraph(Paragraph paragraph, Element after) {
    long handle =
        splitParagraphNative(
            handle(), paragraph.handle(), after == null ? 0 : after.handle());
    return new Paragraph(handle, this);
  }

  /** Takes the children of the paragraph after this one, which then goes. */
  public void mergeParagraphWithNext(Paragraph paragraph) {
    mergeParagraphWithNextNative(handle(), paragraph.handle());
  }

  /** An empty paragraph after this one, of the same style. */
  public Paragraph insertParagraphAfter(Paragraph paragraph) {
    return new Paragraph(insertParagraphAfterNative(handle(), paragraph.handle()), this);
  }

  private static native void destroy(long handle);

  private native void editNative(long handle, String diff);

  private native boolean isEditableNative(long handle);

  private native boolean isSavableNative(long handle, boolean encrypted);

  private native void saveNative(long handle, String path);

  private native void saveEncryptedNative(long handle, String path, String password);

  private native byte[] saveToMemoryNative(long handle);

  private native byte[] saveToMemoryEncryptedNative(long handle, String password);

  private native int fileTypeNative(long handle);

  private native int documentTypeNative(long handle);

  private native long rootElementNative(long handle);

  private native long asFilesystemNative(long handle);

  private native long elementByIdNative(long handle, long identifier);

  private native void removeNative(long handle, long elementHandle);

  private native long insertTextBeforeNative(long handle, long anchorHandle, String text);

  private native long insertTextAfterNative(long handle, long anchorHandle, String text);

  private native long appendTextNative(long handle, long parentHandle, String text);

  private native long splitParagraphNative(long handle, long paragraphHandle, long afterHandle);

  private native void mergeParagraphWithNextNative(long handle, long paragraphHandle);

  private native long insertParagraphAfterNative(long handle, long paragraphHandle);
}
