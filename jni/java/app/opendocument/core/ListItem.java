package app.opendocument.core;

/** List item element. Mirrors {@code odr::ListItem}. */
public final class ListItem extends Element {
  ListItem(long handle, Object owner) {
    super(handle, owner);
  }

  public TextStyle style() {
    return styleNative(handle());
  }

  /** The resolved label, or empty where the list style asks for none. */
  public String marker() {
    return markerNative(handle());
  }

  /** The counter behind {@link #marker}, {@code null} for an unordered item. */
  public Integer number() {
    return numberNative(handle());
  }

  private native TextStyle styleNative(long handle);

  private native String markerNative(long handle);

  private native Integer numberNative(long handle);
}
