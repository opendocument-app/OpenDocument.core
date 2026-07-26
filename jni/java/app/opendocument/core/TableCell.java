package app.opendocument.core;

/** Table cell element. Mirrors {@code odr::TableCell}. */
public final class TableCell extends Element {
  TableCell(long handle, Object owner) {
    super(handle, owner);
  }

  public boolean isCovered() {
    return isCoveredNative(handle());
  }

  public TableDimensions span() {
    return spanNative(handle());
  }

  public ValueType valueType() {
    return ValueType.fromNative(valueTypeNative(handle()));
  }

  public TableCellStyle style() {
    return styleNative(handle());
  }

  private native boolean isCoveredNative(long handle);

  private native TableDimensions spanNative(long handle);

  private native int valueTypeNative(long handle);

  private native TableCellStyle styleNative(long handle);
}
