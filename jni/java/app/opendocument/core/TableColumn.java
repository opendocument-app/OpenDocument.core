package app.opendocument.core;

/** Table column element. Mirrors {@code odr::TableColumn}. */
public final class TableColumn extends Element {
  TableColumn(long handle, Object owner) {
    super(handle, owner);
  }

  public TableColumnStyle style() {
    return styleNative(handle());
  }

  private native TableColumnStyle styleNative(long handle);
}
