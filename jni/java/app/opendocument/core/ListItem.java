package app.opendocument.core;

/** List item element. Mirrors {@code odr::ListItem}. */
public final class ListItem extends Element {
  ListItem(long handle, Object owner) {
    super(handle, owner);
  }

  public TextStyle style() {
    return styleNative(handle());
  }

  private native TextStyle styleNative(long handle);
}
