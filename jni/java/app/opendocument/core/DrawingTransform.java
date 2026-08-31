package app.opendocument.core;

import java.util.Objects;

/** The affine transform a drawing shape carries. Mirrors {@code odr::DrawingTransform}. */
public final class DrawingTransform {
  public final double a;
  public final double b;
  public final double c;
  public final double d;
  public final Measure e;
  public final Measure f;

  public DrawingTransform(double a, double b, double c, double d, Measure e, Measure f) {
    this.a = a;
    this.b = b;
    this.c = c;
    this.d = d;
    this.e = Objects.requireNonNull(e);
    this.f = Objects.requireNonNull(f);
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof DrawingTransform transform
        && a == transform.a
        && b == transform.b
        && c == transform.c
        && d == transform.d
        && e.equals(transform.e)
        && f.equals(transform.f);
  }

  @Override
  public int hashCode() {
    return Objects.hash(a, b, c, d, e, f);
  }

  @Override
  public String toString() {
    return "matrix(" + a + " " + b + " " + c + " " + d + " " + e + " " + f + ")";
  }
}
