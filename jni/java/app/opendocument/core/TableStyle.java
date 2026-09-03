package app.opendocument.core;

/** Style of a table. Mirrors {@code odr::TableStyle}; fields may be {@code null}. */
public final class TableStyle {
  public final Measure width;
  public final DirectionalString border;
  public final String borderInsideHorizontal;
  public final String borderInsideVertical;

  TableStyle(
      Measure width,
      DirectionalString border,
      String borderInsideHorizontal,
      String borderInsideVertical) {
    this.width = width;
    this.border = border;
    this.borderInsideHorizontal = borderInsideHorizontal;
    this.borderInsideVertical = borderInsideVertical;
  }
}
