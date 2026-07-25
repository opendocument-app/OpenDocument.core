package app.opendocument.core;

/** Per-direction measures (e.g. margins). Any side may be {@code null}. */
public final class DirectionalMeasure {
  public final Measure right;
  public final Measure top;
  public final Measure left;
  public final Measure bottom;

  public DirectionalMeasure(Measure right, Measure top, Measure left, Measure bottom) {
    this.right = right;
    this.top = top;
    this.left = left;
    this.bottom = bottom;
  }
}
