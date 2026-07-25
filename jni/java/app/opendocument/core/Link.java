package app.opendocument.core;

/** Link element. Mirrors {@code odr::Link}. */
public final class Link extends Element {
  Link(long handle, Object owner) {
    super(handle, owner);
  }

  public String href() {
    return hrefNative(handle());
  }

  private native String hrefNative(long handle);
}
