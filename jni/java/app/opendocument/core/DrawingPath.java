package app.opendocument.core;

import java.util.Objects;

/** A drawing shape's outline, as an svg path. Mirrors {@code odr::DrawingPath}. */
public final class DrawingPath {
  public final String data;
  public final double x;
  public final double y;
  public final double width;
  public final double height;

  public DrawingPath(String data, double x, double y, double width, double height) {
    this.data = Objects.requireNonNull(data);
    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof DrawingPath path
        && data.equals(path.data)
        && x == path.x
        && y == path.y
        && width == path.width
        && height == path.height;
  }

  @Override
  public int hashCode() {
    return Objects.hash(data, x, y, width, height);
  }

  @Override
  public String toString() {
    return data;
  }
}
