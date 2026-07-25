package app.opendocument.core;

/** Style of a graphic. Mirrors {@code odr::GraphicStyle}; fields may be {@code null}. */
public final class GraphicStyle {
  public final Measure strokeWidth;
  public final Color strokeColor;
  public final Color fillColor;
  public final VerticalAlign verticalAlign;
  public final TextWrap textWrap;

  GraphicStyle(
      Measure strokeWidth, Color strokeColor, Color fillColor, int verticalAlign, int textWrap) {
    this.strokeWidth = strokeWidth;
    this.strokeColor = strokeColor;
    this.fillColor = fillColor;
    this.verticalAlign = VerticalAlign.fromNative(verticalAlign);
    this.textWrap = TextWrap.fromNative(textWrap);
  }
}
