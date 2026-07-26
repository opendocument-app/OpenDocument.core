package app.opendocument.core;

import java.util.List;

/** Table element. Mirrors {@code odr::Table}. */
public final class Table extends Element {
  Table(long handle, Object owner) {
    super(handle, owner);
  }

  public TableRow firstRow() {
    long h = firstRowNative(handle());
    return h == 0 ? null : new TableRow(h, owner());
  }

  public TableColumn firstColumn() {
    long h = firstColumnNative(handle());
    return h == 0 ? null : new TableColumn(h, owner());
  }

  public List<Element> columns() {
    return wrapAll(columnsNative(handle()));
  }

  public List<Element> rows() {
    return wrapAll(rowsNative(handle()));
  }

  public TableDimensions dimensions() {
    return dimensionsNative(handle());
  }

  public TableStyle style() {
    return styleNative(handle());
  }

  private native long firstRowNative(long handle);

  private native long firstColumnNative(long handle);

  private native long[] columnsNative(long handle);

  private native long[] rowsNative(long handle);

  private native TableDimensions dimensionsNative(long handle);

  private native TableStyle styleNative(long handle);
}
