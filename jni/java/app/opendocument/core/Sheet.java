package app.opendocument.core;

import java.util.List;

/** Sheet element of a spreadsheet. Mirrors {@code odr::Sheet}. */
public final class Sheet extends Element {
  Sheet(long handle, Object owner) {
    super(handle, owner);
  }

  public String name() {
    return nameNative(handle());
  }

  /** The paper the sheet is printed on, where the file states one. */
  public PageLayout pageLayout() {
    return pageLayoutNative(handle());
  }

  public TableDimensions dimensions() {
    return dimensionsNative(handle());
  }

  /** Dimensions of the content, optionally clamped to {@code range}. */
  public TableDimensions content(TableDimensions range) {
    if (range == null) {
      return contentNative(handle(), -1, -1);
    }
    return contentNative(handle(), range.rows, range.columns);
  }

  public SheetCell cell(int column, int row) {
    long h = cellNative(handle(), column, row);
    return h == 0 ? null : new SheetCell(h, owner());
  }

  public List<Element> shapes() {
    return wrapAll(shapesNative(handle()));
  }

  public TableStyle style() {
    return styleNative(handle());
  }

  public TableColumnStyle columnStyle(int column) {
    return columnStyleNative(handle(), column);
  }

  public TableRowStyle rowStyle(int row) {
    return rowStyleNative(handle(), row);
  }

  public TableCellStyle cellStyle(int column, int row) {
    return cellStyleNative(handle(), column, row);
  }

  private native String nameNative(long handle);

  private native PageLayout pageLayoutNative(long handle);

  private native TableDimensions dimensionsNative(long handle);

  private native TableDimensions contentNative(long handle, int rows, int columns);

  private native long cellNative(long handle, int column, int row);

  private native long[] shapesNative(long handle);

  private native TableStyle styleNative(long handle);

  private native TableColumnStyle columnStyleNative(long handle, int column);

  private native TableRowStyle rowStyleNative(long handle, int row);

  private native TableCellStyle cellStyleNative(long handle, int column, int row);
}
