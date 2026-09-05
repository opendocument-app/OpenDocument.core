package app.opendocument.core;

import java.util.Objects;

/** The two ends of a line shape. Mirrors {@code odr::DrawingLine}. */
public final class DrawingLine {
  public final Measure x1;
  public final Measure y1;
  public final Measure x2;
  public final Measure y2;

  public DrawingLine(Measure x1, Measure y1, Measure x2, Measure y2) {
    this.x1 = Objects.requireNonNull(x1);
    this.y1 = Objects.requireNonNull(y1);
    this.x2 = Objects.requireNonNull(x2);
    this.y2 = Objects.requireNonNull(y2);
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof DrawingLine line
        && x1.equals(line.x1)
        && y1.equals(line.y1)
        && x2.equals(line.x2)
        && y2.equals(line.y2);
  }

  @Override
  public int hashCode() {
    return Objects.hash(x1, y1, x2, y2);
  }

  @Override
  public String toString() {
    return "(" + x1 + ", " + y1 + ") - (" + x2 + ", " + y2 + ")";
  }
}
