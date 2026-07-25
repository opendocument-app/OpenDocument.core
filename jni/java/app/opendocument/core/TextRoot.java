package app.opendocument.core;

/** Root element of a text document. Mirrors {@code odr::TextRoot}. */
public final class TextRoot extends Element {
  TextRoot(long handle, Object owner) {
    super(handle, owner);
  }

  public PageLayout pageLayout() {
    return pageLayoutNative(handle());
  }

  public MasterPage firstMasterPage() {
    long h = firstMasterPageNative(handle());
    return h == 0 ? null : new MasterPage(h, owner());
  }

  private native PageLayout pageLayoutNative(long handle);

  private native long firstMasterPageNative(long handle);
}
