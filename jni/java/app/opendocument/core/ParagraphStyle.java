package app.opendocument.core;

/**
 * Style of a paragraph. Mirrors {@code odr::ParagraphStyle}; a {@code null} field is one the
 * document does not state. A caller builds one for {@link Paragraph#setStyle}: every field left
 * {@code null} is left alone on the paragraph.
 */
public final class ParagraphStyle {
  public TextAlign textAlign;
  /** The base direction the paragraph's text runs in; {@code null} if the style says nothing. */
  public TextDirection direction;
  public DirectionalMeasure margin;
  public Measure lineHeight;
  public Measure textIndent;
  /** A break the author put before the paragraph; {@code null} if the style says nothing. */
  public BreakType breakBefore;
  /** A break the author put after the paragraph; {@code null} if the style says nothing. */
  public BreakType breakAfter;

  /** Every field {@code null}. */
  public ParagraphStyle() {}

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
