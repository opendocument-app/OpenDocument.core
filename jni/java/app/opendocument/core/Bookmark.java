package app.opendocument.core;

/** Bookmark element. Mirrors {@code odr::Bookmark}. */
public final class Bookmark extends Element {
  Bookmark(long handle, Object owner) {
    super(handle, owner);
  }

  public String name() {
    return nameNative(handle());
  }

  private native String nameNative(long handle);
}
