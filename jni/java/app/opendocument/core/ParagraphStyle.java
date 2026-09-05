package app.opendocument.core;

/** Style of a paragraph. Mirrors {@code odr::ParagraphStyle}; fields may be {@code null}. */
public final class ParagraphStyle {
  public final TextAlign textAlign;
  /** The base direction the paragraph's text runs in; {@code null} if the style says nothing. */
  public final TextDirection direction;
  public final DirectionalMeasure margin;
  public final Measure lineHeight;
  public final Measure textIndent;
  /** A break the author put before the paragraph; {@code null} if the style says nothing. */
  public final BreakType breakBefore;
  /** A break the author put after the paragraph; {@code null} if the style says nothing. */
  public final BreakType breakAfter;

  ParagraphStyle(
      int textAlign,
      int direction,
      DirectionalMeasure margin,
      Measure lineHeight,
      Measure textIndent,
      int breakBefore,
      int breakAfter) {
    this.textAlign = TextAlign.fromNative(textAlign);
    this.direction = TextDirection.fromNative(direction);
    this.margin = margin;
    this.lineHeight = lineHeight;
    this.textIndent = textIndent;
    this.breakBefore = BreakType.fromNative(breakBefore);
    this.breakAfter = BreakType.fromNative(breakAfter);
  }
}
