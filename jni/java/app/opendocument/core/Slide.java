package app.opendocument.core;

/** Slide element of a presentation. Mirrors {@code odr::Slide}. */
public final class Slide extends Element {
  Slide(long handle, Object owner) {
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

  /** The fill the page shows: its own, else its master page's. */
  public GraphicStyle background() {
    return backgroundNative(handle());
  }

  private native String nameNative(long handle);

  private native PageLayout pageLayoutNative(long handle);

  private native long masterPageNative(long handle);

  private native GraphicStyle backgroundNative(long handle);
}
