package app.opendocument.core;

import java.util.Objects;

/** A text placeholder of a slide layout, positioned on the page. Mirrors {@code odr::SlidePlaceholder}. */
public final class SlidePlaceholder {
  public final SlidePlaceholderKind kind;
  public final Measure x;
  public final Measure y;
  public final Measure width;
  public final Measure height;

  public SlidePlaceholder(
      SlidePlaceholderKind kind, Measure x, Measure y, Measure width, Measure height) {
    this.kind = Objects.requireNonNull(kind);
    this.x = Objects.requireNonNull(x);
    this.y = Objects.requireNonNull(y);
    this.width = Objects.requireNonNull(width);
    this.height = Objects.requireNonNull(height);
  }

  SlidePlaceholder(int kind, Measure x, Measure y, Measure width, Measure height) {
    this(SlidePlaceholderKind.fromNative(kind), x, y, width, height);
  }
}
