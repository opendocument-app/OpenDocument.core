package app.opendocument.core;

import java.util.Objects;

/** The shadow a shape casts. Mirrors {@code odr::Shadow}. */
public final class Shadow {
  public final Measure offsetX;
  public final Measure offsetY;
  /** The color, with the opacity in its alpha. */
  public final Color color;
  public final Measure blur;

  Shadow(Measure offsetX, Measure offsetY, Color color, Measure blur) {
    this.offsetX = Objects.requireNonNull(offsetX);
    this.offsetY = Objects.requireNonNull(offsetY);
    this.color = Objects.requireNonNull(color);
    this.blur = Objects.requireNonNull(blur);
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof Shadow shadow
        && offsetX.equals(shadow.offsetX)
        && offsetY.equals(shadow.offsetY)
        && color.equals(shadow.color)
        && blur.equals(shadow.blur);
  }

  @Override
  public int hashCode() {
    return Objects.hash(offsetX, offsetY, color, blur);
  }

  @Override
  public String toString() {
    return offsetX + " " + offsetY + " " + blur + " " + color;
  }
}
