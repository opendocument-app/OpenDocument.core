package app.opendocument.core;

/** Layout of a page. Mirrors {@code odr::PageLayout}; fields may be {@code null}. */
public final class PageLayout {
  public final Measure width;
  public final Measure height;
  public final PrintOrientation printOrientation;
  public final DirectionalMeasure margin;
  public final Color backgroundColor;

  PageLayout(
      Measure width,
      Measure height,
      int printOrientation,
      DirectionalMeasure margin,
      Color backgroundColor) {
    this.width = width;
    this.height = height;
    this.printOrientation = PrintOrientation.fromNative(printOrientation);
    this.margin = margin;
    this.backgroundColor = backgroundColor;
  }
}
