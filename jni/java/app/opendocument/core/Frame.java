package app.opendocument.core;

/** Frame element. Mirrors {@code odr::Frame}; geometry values may be {@code null}. */
public final class Frame extends Element {
  Frame(long handle, Object owner) {
    super(handle, owner);
  }

  public AnchorType anchorType() {
    return AnchorType.fromNative(anchorTypeNative(handle()));
  }

  public Measure x() {
    return xNative(handle());
  }

  public Measure y() {
    return yNative(handle());
  }

  public Measure width() {
    return widthNative(handle());
  }

  public Measure height() {
    return heightNative(handle());
  }

  public Integer zIndex() {
    return zIndexNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native int anchorTypeNative(long handle);

  private native Measure xNative(long handle);

  private native Measure yNative(long handle);

  private native Measure widthNative(long handle);

  private native Measure heightNative(long handle);

  private native Integer zIndexNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
