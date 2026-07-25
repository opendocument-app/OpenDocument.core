package app.opendocument.core;

/** Style of a paragraph. Mirrors {@code odr::ParagraphStyle}; fields may be {@code null}. */
public final class ParagraphStyle {
  public final TextAlign textAlign;
  public final DirectionalMeasure margin;
  public final Measure lineHeight;
  public final Measure textIndent;

  ParagraphStyle(int textAlign, DirectionalMeasure margin, Measure lineHeight, Measure textIndent) {
    this.textAlign = TextAlign.fromNative(textAlign);
    this.margin = margin;
    this.lineHeight = lineHeight;
    this.textIndent = textIndent;
  }
}
