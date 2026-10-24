package app.opendocument.core;

/** Cell element of a sheet. Mirrors {@code odr::SheetCell}. */
public final class SheetCell extends Element {
  SheetCell(long handle, Object owner) {
    super(handle, owner);
  }

  public TablePosition position() {
    return positionNative(handle());
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

  /** The format code the cell shows a number in, such as {@code 0.00%}, or null for General. */
  public String numberFormat() {
    return numberFormatNative(handle());
  }

  private native TablePosition positionNative(long handle);

  private native boolean isCoveredNative(long handle);

  private native TableDimensions spanNative(long handle);

  private native int valueTypeNative(long handle);

  private native String numberFormatNative(long handle);
}
