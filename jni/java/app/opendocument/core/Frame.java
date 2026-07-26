package app.opendocument.core;

/** Frame element. Mirrors {@code odr::Frame}; geometry values may be {@code null}. */
public final class Frame extends Element {
  Frame(long handle, Object owner) {
    super(handle, owner);
  }

  public AnchorType anchorType() {
    return AnchorType.fromNative(anchorTypeNative(handle()));
  }

  public String x() {
    return xNative(handle());
  }

  public String y() {
    return yNative(handle());
  }

  public String width() {
    return widthNative(handle());
  }

  public String height() {
    return heightNative(handle());
  }

  public String zIndex() {
    return zIndexNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native int anchorTypeNative(long handle);

  private native String xNative(long handle);

  private native String yNative(long handle);

  private native String widthNative(long handle);

  private native String heightNative(long handle);

  private native String zIndexNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
