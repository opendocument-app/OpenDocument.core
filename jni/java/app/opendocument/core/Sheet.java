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

  /**
   * States the non-null fields of both styles on the cell and leaves the rest. The fill, the
   * horizontal alignment and the text keys of {@link Text#setStyle} are written; any other field
   * throws {@link OdrException.UnsupportedOperation}.
   */
  public void setCellStyle(int column, int row, TableCellStyle cellStyle, TextStyle textStyle) {
    setCellStyleNative(handle(), column, row, cellStyle, textStyle);
  }

  /**
   * {@link #setCellStyle} on every cell of the row, also the ones past what the file states.
   */
  public void setRowStyle(int row, TableCellStyle cellStyle, TextStyle textStyle) {
    setRowStyleNative(handle(), row, cellStyle, textStyle);
  }

  /**
   * {@link #setCellStyle} on every cell of the column, also the ones past what the file states.
   */
  public void setColumnStyle(int column, TableCellStyle cellStyle, TextStyle textStyle) {
    setColumnStyleNative(handle(), column, cellStyle, textStyle);
  }

  /**
   * Moves the rows from {@code row} on down by {@code count}, and every reference to them in the
   * document with them. The new rows are empty.
   */
  public void insertRows(int row, int count) {
    insertRowsNative(handle(), row, count);
  }

  /**
   * Removes {@code count} rows from {@code row} on, and moves the rows below up. A reference into
   * the removed rows becomes {@code #REF!}.
   */
  public void deleteRows(int row, int count) {
    deleteRowsNative(handle(), row, count);
  }

  /**
   * Moves the columns from {@code column} on right by {@code count}, and every reference to them
   * in the document with them. The new columns are empty.
   */
  public void insertColumns(int column, int count) {
    insertColumnsNative(handle(), column, count);
  }

  /**
   * Removes {@code count} columns from {@code column} on, and moves the columns right of them
   * left. A reference into the removed columns becomes {@code #REF!}.
   */
  public void deleteColumns(int column, int count) {
    deleteColumnsNative(handle(), column, count);
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

  private native void setCellStyleNative(
      long handle, int column, int row, TableCellStyle cellStyle, TextStyle textStyle);

  private native void setRowStyleNative(
      long handle, int row, TableCellStyle cellStyle, TextStyle textStyle);

  private native void setColumnStyleNative(
      long handle, int column, TableCellStyle cellStyle, TextStyle textStyle);

  private native void insertRowsNative(long handle, int row, int count);

  private native void deleteRowsNative(long handle, int row, int count);

  private native void insertColumnsNative(long handle, int column, int count);

  private native void deleteColumnsNative(long handle, int column, int count);
}
