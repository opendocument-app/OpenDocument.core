package app.opendocument.core;

/** Span element. Mirrors {@code odr::Span}. */
public final class Span extends Element {
  Span(long handle, Object owner) {
    super(handle, owner);
  }

  public TextStyle style() {
    return styleNative(handle());
  }

  private native TextStyle styleNative(long handle);
}
