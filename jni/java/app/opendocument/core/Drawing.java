package app.opendocument.core;

/** A drawing: its pages. Mirrors {@code odr::Drawing}. */
public final class Drawing extends Document {
  Drawing(long handle) {
    super(handle);
  }

  /**
   * A text box with one empty paragraph on a drawing page, drawing no fill and no outline. Every
   * length needs a fixed unit, and the size must be positive.
   */
  public Frame insertTextBox(Element page, Measure x, Measure y, Measure width, Measure height) {
    try {
      return new Frame(insertTextBoxNative(handle(), page.handle(), x, y, width, height), this);
    } finally {
      page.keepAlive();
    }
  }

  private native long insertTextBoxNative(
      long handle, long pageHandle, Measure x, Measure y, Measure width, Measure height);
}
