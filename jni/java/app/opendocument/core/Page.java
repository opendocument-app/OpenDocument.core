package app.opendocument.core;

/** Page element of a drawing. Mirrors {@code odr::Page}. */
public final class Page extends Element {
  Page(long handle, Object owner) {
    super(handle, owner);
  }

  public String name() {
    return nameNative(handle());
  }

  public PageLayout pageLayout() {
    return pageLayoutNative(handle());
  }

  public MasterPage masterPage() {
    long h = masterPageNative(handle());
    return h == 0 ? null : new MasterPage(h, owner());
  }

  private native String nameNative(long handle);

  private native PageLayout pageLayoutNative(long handle);

  private native long masterPageNative(long handle);
}
