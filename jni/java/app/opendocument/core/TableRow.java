package app.opendocument.core;

/** Table row element. Mirrors {@code odr::TableRow}. */
public final class TableRow extends Element {
  TableRow(long handle, Object owner) {
    super(handle, owner);
  }

  public TableRowStyle style() {
    return styleNative(handle());
  }

  private native TableRowStyle styleNative(long handle);
}
