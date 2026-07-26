package app.opendocument.core;

/** Master page element. Mirrors {@code odr::MasterPage}. */
public final class MasterPage extends Element {
  MasterPage(long handle, Object owner) {
    super(handle, owner);
  }

  public PageLayout pageLayout() {
    return pageLayoutNative(handle());
  }

  private native PageLayout pageLayoutNative(long handle);
}
